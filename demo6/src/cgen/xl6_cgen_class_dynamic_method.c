/*
 * class 运行时方法元数据与 XCall 桥。
 *
 * 静态调用仍走 typed HIR 选定的直接/虚调用路径；只有接收者静态类型为 var 时，
 * 才通过这里生成的 xrt_method_table 完成运行时重载选择。两条路径共享同一方法签名。
 */

static int xl6CgenDynamicMethodEligible(const XL6_AstNode* method)
{
    if ( method == NULL ||
         (method->kind != XL6_AST_FUNC &&
          method->kind != XL6_AST_EXPORT_FUNC &&
          method->kind != XL6_AST_NATIVE_EXTERN_FUNC) ||
         xl6CgenIsStaticMember(method) ||
         xl6CgenAstHasFlag(method, XL6_AST_FLAG_PRIVATE) ||
         xl6CgenAstHasFlag(method, XL6_AST_FLAG_PROTECTED) ||
         xl6CgenFuncIsGeneric(method) ) {
        return 0;
    }
    return xl6CgenCallableName(method) != NULL;
}

static size_t xl6CgenDynamicMethodCount(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body = xl6CgenClassBody(classNode);
    size_t count = 0;
    size_t index;

    for ( index = 0; body != NULL && index < body->childCount; ++index ) {
        if ( xl6CgenDynamicMethodEligible(body->children[index]) ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenDynamicMethodXCallSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* method)
{
    xl6OutputPuts("xl6_method_xcall_", ctx->out);
    if ( typeNode != NULL ) {
        xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, method);
    } else {
        xl6CgenClassCallableSymbol(ctx, classNode, method);
    }
}

static void xl6CgenDynamicMethodTableSymbol(XL6_CgenContext* ctx, const char* cTypeName)
{
    xl6OutputPuts("xl6_method_table_", ctx->out);
    xl6CgenFputsCIdent(ctx->out, cTypeName != NULL ? cTypeName : "_");
}

static void xl6CgenEmitDynamicMethodXCallProtos(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* body;
    size_t index;

    if ( ctx == NULL || classNode == NULL || !ctx->needsFunctionVarRuntime ) {
        return;
    }
    body = xl6CgenClassBody(classNode);
    for ( index = 0; body != NULL && index < body->childCount; ++index ) {
        const XL6_AstNode* method = body->children[index];
        if ( !xl6CgenDynamicMethodEligible(method) ||
             !xl6CgenCallableSupportsXCall(
                 ctx,
                 xl6CgenCallableParams(method),
                 method->childCount > 2 ? method->children[2] : NULL) ) {
            continue;
        }
        xl6OutputPuts("static bool ", ctx->out);
        xl6CgenDynamicMethodXCallSymbol(ctx, classNode, typeNode, method);
        xl6OutputPuts("(xrt_call_frame* frame, xrt_call_result* result);\n", ctx->out);
    }
}

static void xl6CgenEmitDynamicMethodTableProto(
    XL6_CgenContext* ctx,
    const char* cTypeName)
{
    if ( ctx == NULL || cTypeName == NULL ) {
        return;
    }
    xl6OutputPuts("static const xrt_method_table* ", ctx->out);
    xl6CgenDynamicMethodTableSymbol(ctx, cTypeName);
    xl6OutputPuts("(void);\n", ctx->out);
}

static void xl6CgenEmitDynamicMethodTable(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode,
    const char* cTypeName)
{
    const XL6_AstNode* body;
    size_t methodCount;
    size_t bodyIndex;
    size_t methodIndex;

    if ( ctx == NULL || classNode == NULL || cTypeName == NULL ) {
        return;
    }
    methodCount = xl6CgenDynamicMethodCount(classNode);
    body = xl6CgenClassBody(classNode);

    if ( methodCount == 0 ) {
        xl6OutputPuts("static const xrt_method_table* ", ctx->out);
        xl6CgenDynamicMethodTableSymbol(ctx, cTypeName);
        xl6OutputPuts("(void) { return 0; }\n\n", ctx->out);
        return;
    }

    xl6OutputPuts("static xrt_method_desc xl6_methods_", ctx->out);
    xl6CgenFputsCIdent(ctx->out, cTypeName);
    xl6OutputPrintf(ctx->out, "[%uu];\n", (unsigned)methodCount);
    xl6OutputPuts("static xrt_method_table xl6_methods_table_", ctx->out);
    xl6CgenFputsCIdent(ctx->out, cTypeName);
    xl6OutputPrintf(ctx->out, " = { %uu, xl6_methods_", (unsigned)methodCount);
    xl6CgenFputsCIdent(ctx->out, cTypeName);
    xl6OutputPuts(" };\n", ctx->out);
    xl6OutputPuts("static xrt_once xl6_methods_once_", ctx->out);
    xl6CgenFputsCIdent(ctx->out, cTypeName);
    xl6OutputPuts(" = XRT_ONCE_INIT;\n", ctx->out);

    xl6OutputPuts("static void xl6_methods_init_", ctx->out);
    xl6CgenFputsCIdent(ctx->out, cTypeName);
    xl6OutputPuts("(void* context)\n{\n    (void)context;\n", ctx->out);
    methodIndex = 0;
    for ( bodyIndex = 0; body != NULL && bodyIndex < body->childCount; ++bodyIndex ) {
        const XL6_AstNode* method = body->children[bodyIndex];
        const XL6_AstNode* params;
        const XL6_AstNode* returns;
        const char* name;
        char sigName[64];
        unsigned long long signatureId;
        int supportsXCall;

        if ( !xl6CgenDynamicMethodEligible(method) ) {
            continue;
        }
        params = xl6CgenCallableParams(method);
        returns = method->childCount > 2 ? method->children[2] : NULL;
        name = xl6CgenCallableName(method);
        signatureId = xl6CgenFunctionSignatureId(ctx, params, returns);
        supportsXCall = ctx->needsFunctionVarRuntime &&
            xl6CgenCallableSupportsXCall(ctx, params, returns);
        snprintf(sigName, sizeof(sigName), "__xl6_method_sig_%u", (unsigned)methodIndex);
        xl6CgenEmitFunctionSigObject(ctx, sigName, name, signatureId, params, returns);

        xl6OutputPuts("    xl6_methods_", ctx->out);
        xl6CgenFputsCIdent(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "[%u].Name = ", (unsigned)methodIndex);
        xl6CgenCStringLiteral(ctx->out, name);
        xl6OutputPuts(";\n", ctx->out);
        xl6OutputPuts("    xl6_methods_", ctx->out);
        xl6CgenFputsCIdent(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "[%u].NameSize = %uu;\n", (unsigned)methodIndex, (unsigned)strlen(name));
        xl6OutputPuts("    xl6_methods_", ctx->out);
        xl6CgenFputsCIdent(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "[%u].Sig = &%s;\n", (unsigned)methodIndex, sigName);
        xl6OutputPuts("    xl6_methods_", ctx->out);
        xl6CgenFputsCIdent(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "[%u].NativeEntry = (void*)", (unsigned)methodIndex);
        if ( typeNode != NULL ) {
            xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, method);
        } else {
            xl6CgenClassCallableSymbol(ctx, classNode, method);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6OutputPuts("    xl6_methods_", ctx->out);
        xl6CgenFputsCIdent(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "[%u].NativeAbi = 1u;\n", (unsigned)methodIndex);
        xl6OutputPuts("    xl6_methods_", ctx->out);
        xl6CgenFputsCIdent(ctx->out, cTypeName);
        xl6OutputPrintf(ctx->out, "[%u].XCallEntry = ", (unsigned)methodIndex);
        if ( supportsXCall ) {
            xl6CgenDynamicMethodXCallSymbol(ctx, classNode, typeNode, method);
        } else {
            xl6OutputPuts("0", ctx->out);
        }
        xl6OutputPuts(";\n", ctx->out);
        methodIndex += 1;
    }
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("static const xrt_method_table* ", ctx->out);
    xl6CgenDynamicMethodTableSymbol(ctx, cTypeName);
    xl6OutputPuts("(void)\n{\n    xrtOnceRunGraph(&xl6_methods_once_", ctx->out);
    xl6CgenFputsCIdent(ctx->out, cTypeName);
    xl6OutputPuts(", xl6_methods_init_", ctx->out);
    xl6CgenFputsCIdent(ctx->out, cTypeName);
    xl6OutputPuts(", 0);\n    return &xl6_methods_table_", ctx->out);
    xl6CgenFputsCIdent(ctx->out, cTypeName);
    xl6OutputPuts(";\n}\n\n", ctx->out);
}

static void xl6CgenEmitDynamicMethodSelf(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode)
{
    char cTypeName[512];

    if ( typeNode != NULL ) {
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
    } else {
        xl6CgenClassTypeTextName(cTypeName, sizeof(cTypeName), classNode->text);
    }
    xl6OutputPuts("    xvalue __xl6_self_value;\n", ctx->out);
    if ( xl6CgenClassIsPureNativeHandle(classNode) ) {
        xl6OutputPrintf(ctx->out, "    %s __xl6_self_storage;\n", cTypeName);
        xl6OutputPrintf(ctx->out, "    %s* self;\n", cTypeName);
    } else {
        xl6OutputPrintf(ctx->out, "    %s* self;\n", cTypeName);
    }
    xl6OutputPuts("    if ( frame == 0 || result == 0 || frame->Self == 0 ||\n", ctx->out);
    xl6OutputPuts("         !xrtTypeIsA(xvoTypeDesc(frame->Self), ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("()) ) { return false; }\n", ctx->out);
    xl6OutputPuts("    __xl6_self_value = frame->Self;\n", ctx->out);
    if ( xl6CgenClassIsPureNativeHandle(classNode) ) {
        xl6OutputPuts("    __xl6_self_storage.handle = xvoGetHandleData(__xl6_self_value);\n", ctx->out);
        xl6OutputPuts("    self = &__xl6_self_storage;\n", ctx->out);
    } else {
        xl6OutputPrintf(ctx->out, "    self = (%s*)xvoGetRecordData(__xl6_self_value);\n", cTypeName);
        xl6OutputPuts("    if ( self == 0 ) { return false; }\n", ctx->out);
    }
}

static void xl6CgenEmitDynamicMethodDefinition(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* method)
{
    const XL6_AstNode* params = xl6CgenCallableParams(method);
    const XL6_AstNode* returns = method->childCount > 2 ? method->children[2] : NULL;
    int fixedCount = xl6CgenFixedParamCount(params);
    int hasArrayVararg = xl6CgenParamListHasArrayVararg(params, fixedCount);
    int hasDictVararg = xl6CgenParamListHasDictVararg(params, fixedCount);
    size_t index;

    xl6OutputPuts("static bool ", ctx->out);
    xl6CgenDynamicMethodXCallSymbol(ctx, classNode, typeNode, method);
    xl6OutputPuts("(xrt_call_frame* frame, xrt_call_result* result)\n{\n", ctx->out);
    xl6CgenEmitXCallLocals(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
    xl6CgenEmitDynamicMethodSelf(ctx, classNode, typeNode);
    xl6OutputPuts("    xrt_call_frame* __xl6_frame = frame;\n", ctx->out);
    xl6OutputPuts("    xrt_call_result* __xl6_result = result;\n", ctx->out);
    xl6CgenEmitXCallSetup(ctx);
    xl6CgenEmitXCallParamSetup(ctx, params, fixedCount);
    xl6CgenEmitXCallArityCheck(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
    xl6CgenEmitXCallTypeChecks(ctx, params, fixedCount);
    xl6CgenEmitXCallVarargPack(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);

    if ( returns != NULL && returns->childCount > 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6CgenFuncReturnStructName(ctx, method);
        xl6OutputPuts(" __xl6_multi = ", ctx->out);
        if ( typeNode != NULL ) {
            xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, method);
        } else {
            xl6CgenClassCallableSymbol(ctx, classNode, method);
        }
        xl6OutputPuts("(self", ctx->out);
        xl6CgenEmitXCallCallArgs(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenEmitCallableBridgeErrorReturn(ctx, hasArrayVararg, hasDictVararg, "false");
        for ( index = 0; index < returns->childCount; ++index ) {
            char valueExpr[96];
            snprintf(valueExpr, sizeof(valueExpr), "__xl6_multi.value%u", (unsigned)index);
            xl6CgenEmitXCallSetResult(ctx, returns->children[index], (unsigned)index, valueExpr, hasArrayVararg, hasDictVararg);
        }
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
        xl6OutputPuts(" __xl6_single = ", ctx->out);
        if ( typeNode != NULL ) {
            xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, method);
        } else {
            xl6CgenClassCallableSymbol(ctx, classNode, method);
        }
        xl6OutputPuts("(self", ctx->out);
        xl6CgenEmitXCallCallArgs(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenEmitCallableBridgeErrorReturn(ctx, hasArrayVararg, hasDictVararg, "false");
        xl6CgenEmitXCallSetResult(ctx, returns->children[0], 0u, "__xl6_single", hasArrayVararg, hasDictVararg);
    } else {
        xl6OutputPuts("    ", ctx->out);
        if ( typeNode != NULL ) {
            xl6CgenGenericClassCallableSymbol(ctx, classNode, typeNode, method);
        } else {
            xl6CgenClassCallableSymbol(ctx, classNode, method);
        }
        xl6OutputPuts("(self", ctx->out);
        xl6CgenEmitXCallCallArgs(ctx, params, fixedCount, hasArrayVararg, hasDictVararg);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenEmitCallableBridgeErrorReturn(ctx, hasArrayVararg, hasDictVararg, "false");
    }
    /* vararg/kwargs 作为 owned 参数交给目标方法，由目标方法的统一清理出口释放。 */
    xl6OutputPuts("    return true;\n}\n\n", ctx->out);
}

static void xl6CgenEmitDynamicMethodDefinitions(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t index;

    if ( ctx == NULL || classNode == NULL || !ctx->needsFunctionVarRuntime ||
         xl6CgenClassHasTypeParams(classNode) ) {
        return;
    }
    body = xl6CgenClassBody(classNode);
    for ( index = 0; body != NULL && index < body->childCount; ++index ) {
        const XL6_AstNode* method = body->children[index];
        if ( xl6CgenDynamicMethodEligible(method) &&
            xl6CgenCallableSupportsXCall(
                 ctx,
                 xl6CgenCallableParams(method),
                 method->childCount > 2 ? method->children[2] : NULL) ) {
            xl6CgenEmitDynamicMethodDefinition(ctx, classNode, NULL, method);
        }
    }
}

static void xl6CgenEmitGenericDynamicMethodDefinitions(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* body;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    size_t index;

    if ( ctx == NULL || classNode == NULL || typeNode == NULL ||
         !ctx->needsFunctionVarRuntime ) {
        return;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
    ctx->currentGenericArgs = typeNode->childCount > 0 ? typeNode->children[0] : NULL;

    body = xl6CgenClassBody(classNode);
    for ( index = 0; body != NULL && index < body->childCount; ++index ) {
        const XL6_AstNode* method = body->children[index];
        if ( xl6CgenDynamicMethodEligible(method) &&
             xl6CgenCallableSupportsXCall(
                 ctx,
                 xl6CgenCallableParams(method),
                 method->childCount > 2 ? method->children[2] : NULL) ) {
            xl6CgenEmitDynamicMethodDefinition(ctx, classNode, typeNode, method);
        }
    }

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
}
