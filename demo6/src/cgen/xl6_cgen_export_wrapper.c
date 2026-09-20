static int xl6CgenExportWrapperParamNeedsTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* param)
{
    return param != NULL &&
           param->childCount > 0 &&
           (param->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) == 0 &&
           xl6CgenIsTypedContainerType(ctx, param->children[0]);
}

static int xl6CgenExportWrapperHasParamTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL ) {
        return 0;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        if ( xl6CgenExportWrapperParamNeedsTemp(ctx, params->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenExportWrapperParamTempName(
    char* buffer,
    size_t capacity,
    size_t index)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    snprintf(buffer, capacity, "__xl6_cabi_arg_%u", (unsigned)index);
}

/*
    C 调用方始终拥有传入的类型化容器。包装器创建独立副本，
    内部函数只借用该副本，调用结束后再由包装器统一释放。
*/
static void xl6CgenEmitExportWrapperParamTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        char tempName[64];
        char paramName[512];

        if ( !xl6CgenExportWrapperParamNeedsTemp(ctx, param) ) {
            continue;
        }
        xl6CgenExportWrapperParamTempName(tempName, sizeof(tempName), i);
        xl6CgenCIdentText(paramName, sizeof(paramName), param->text);
        xl6CgenIndent(ctx);
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, param->children[0]), ctx->out);
        xl6OutputPrintf(ctx->out, " %s = ", tempName);
        xl6CgenTypedContainerCloneCExpr(
            ctx,
            param->children[0],
            paramName);
        xl6OutputPuts(";\n", ctx->out);
    }
}

static void xl6CgenEmitExportWrapperParamArg(
    XL6_CgenContext* ctx,
    const XL6_AstNode* param,
    size_t index)
{
    if ( param == NULL ) {
        xl6OutputPuts("0", ctx->out);
        return;
    }
    if ( (param->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) != 0 ) {
        xl6CgenFputsCIdent(ctx->out, param->text);
        return;
    }
    if ( param->childCount > 0 && xl6CgenIsXValueType(param->children[0]) ) {
        xl6OutputPuts("xl6_xrt_retain(", ctx->out);
        xl6CgenFputsCIdent(ctx->out, param->text);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    if ( param->childCount > 0 &&
         xl6CgenIsFunctionType(param->children[0]) ) {
        xl6CgenEmitCAbiCallbackMakeExpr(
            ctx,
            param->children[0],
            param->text);
        return;
    }
    if ( param->childCount > 0 &&
         xl6CgenNullableHasManagedInner(ctx, param->children[0]) ) {
        char copyName[64];

        xl6CgenNullableCopyHelperName(
            copyName,
            sizeof(copyName),
            param->children[0]);
        xl6OutputPrintf(ctx->out, "%s(", copyName);
        xl6CgenFputsCIdent(ctx->out, param->text);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    if ( param->childCount > 0 &&
         xl6CgenIsTaggedEnumType(ctx, param->children[0]) ) {
        xl6CgenEnumOpsSymbol(
            ctx->out,
            param->children[0],
            "copy_value");
        xl6OutputPutc('(', ctx->out);
        xl6CgenFputsCIdent(ctx->out, param->text);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    if ( xl6CgenExportWrapperParamNeedsTemp(ctx, param) ) {
        char tempName[64];

        xl6CgenExportWrapperParamTempName(tempName, sizeof(tempName), index);
        xl6OutputPuts(tempName, ctx->out);
        return;
    }
    xl6CgenFputsCIdent(ctx->out, param->text);
}

static void xl6CgenEmitExportWrapperParamArgs(
    XL6_CgenContext* ctx,
    const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        xl6CgenEmitExportWrapperParamArg(ctx, params->children[i], i);
    }
}

static void xl6CgenEmitExportWrapperParamTempCleanups(
    XL6_CgenContext* ctx,
    const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL ) {
        return;
    }
    for ( i = params->childCount; i > 0; --i ) {
        const XL6_AstNode* param = params->children[i - 1];
        char tempName[64];

        if ( !xl6CgenExportWrapperParamNeedsTemp(ctx, param) ) {
            continue;
        }
        xl6CgenExportWrapperParamTempName(tempName, sizeof(tempName), i - 1);
        xl6CgenIndent(ctx);
        xl6CgenTypedContainerDestroyValue(
            ctx,
            param->children[0],
            tempName);
    }
}
