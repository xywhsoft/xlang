static void xl6CgenFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    const XL6_AstNode* body;
    XL6_AstNode* concreteReturnType;
    size_t varMark;
    size_t cleanupMark;

    xl6CgenFuncProto(ctx, node);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, node);
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    returns = node->childCount > 2 ? node->children[2] : NULL;
    concreteReturnType = returns != NULL && returns->childCount == 1
        ? xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0])
        : NULL;
    ctx->currentReturnType = concreteReturnType != NULL
        ? concreteReturnType
        : (returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL);
    params = node->childCount > 1 ? node->children[1] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = node;
    if ( xl6CgenFuncIsMainEntry(node) ) {
        xl6CgenRegisterBorrowedParamTypes(ctx, params);
    } else {
        xl6CgenRegisterParamTypes(ctx, params);
    }
    xl6CgenEmitManagedParamOwners(ctx, params);
    body = node->childCount > 3 ? node->children[3] : NULL;
    if ( body != NULL ) {
        xl6CgenBlock(ctx, body);
    }
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    if ( xl6CgenFuncIsMainEntry(node) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("return 0;\n", ctx->out);
    } else {
        /* 清理标签也可能成为 C 控制流入口，因此函数尾始终生成一个确定返回。 */
        xl6CgenEmitDefaultErrorReturn(ctx);
    }
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    xl6AstFree(concreteReturnType);
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenExportFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    const XL6_AstNode* body;
    XL6_AstNode* concreteReturnType;
    size_t varMark;
    size_t cleanupMark;

    xl6CgenExportInternalProto(ctx, node);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    xl6CgenBeginTempScope(ctx, node);
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    returns = node->childCount > 2 ? node->children[2] : NULL;
    concreteReturnType = returns != NULL && returns->childCount == 1
        ? xl6CgenCloneCanonicalTypeNode(ctx, returns->children[0])
        : NULL;
    ctx->currentReturnType = concreteReturnType != NULL
        ? concreteReturnType
        : (returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL);
    params = node->childCount > 1 ? node->children[1] : NULL;
    ctx->currentReturnParams = params;
    ctx->currentReturns = returns;
    ctx->currentReturnFunc = node;
    xl6CgenRegisterParamTypes(ctx, params);
    xl6CgenEmitManagedParamOwners(ctx, params);
    body = node->childCount > 3 ? node->children[3] : NULL;
    if ( body != NULL ) {
        xl6CgenBlock(ctx, body);
    }
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }
    xl6CgenEmitDefaultErrorReturn(ctx);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    xl6AstFree(concreteReturnType);
    xl6CgenFreeVarsSince(ctx, varMark);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenExportWrapper(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* returns;
    const XL6_AstNode* params;

    xl6CgenExportWrapperProto(ctx, node);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    returns = node->childCount > 2 ? node->children[2] : NULL;
    xl6CgenIndent(ctx);
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFuncReturnStructName(ctx, node);
        xl6OutputPuts(" __xl6_return = ", ctx->out);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("return ", ctx->out);
    }
    xl6CgenExportInternalSymbol(ctx, node);
    xl6OutputPutc('(', ctx->out);
    params = node->childCount > 1 ? node->children[1] : NULL;
    if ( params != NULL ) {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                xl6OutputPuts(", ", ctx->out);
            }
            if ( param != NULL && param->childCount > 0 && xl6CgenIsXValueType(param->children[0]) ) {
                /* C ABI 参数由调用方拥有，进入内部 owned-parameter ABI 前增加引用。 */
                xl6OutputPuts("xl6_xrt_retain(", ctx->out);
                xl6CgenFputsCIdent(ctx->out, param->text);
                xl6OutputPutc(')', ctx->out);
            } else {
                xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
            }
        }
    }
    xl6OutputPuts(");\n", ctx->out);
    if ( returns != NULL && returns->childCount > 1 ) {
        for ( i = 0; i < returns->childCount; ++i ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if ( out%u != 0 ) {\n", (unsigned)i);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "*out%u = (", (unsigned)i);
            xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[i]), ctx->out);
            xl6OutputPrintf(ctx->out, ")__xl6_return.value%u;\n", (unsigned)i);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
    }
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}
