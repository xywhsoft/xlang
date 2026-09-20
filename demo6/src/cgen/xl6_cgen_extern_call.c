/*
    xlang 内部的 xvalue* 形参 ABI 接收 owned 引用，普通 C/XRT API 则借用 xvalue*。
    仅为包含 xvalue* 参数的 extern 函数生成薄包装器，集中完成这层所有权转换。
*/

static int xl6CgenExternCallNeedsWrapper(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* params;
    size_t i;

    if ( ctx == NULL || node == NULL || node->kind != XL6_AST_EXTERN_FUNC ) {
        return 0;
    }
    params = node->childCount > 1 ? node->children[1] : NULL;
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        XL6_AstNode* typeNode;
        int isXValue;

        if ( param == NULL || param->childCount == 0 ) {
            continue;
        }
        typeNode = xl6CgenCloneCanonicalTypeNode(ctx, param->children[0]);
        isXValue = xl6CgenIsXValueType(typeNode != NULL ? typeNode : param->children[0]);
        xl6AstFree(typeNode);
        if ( isXValue ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenExternCallWrapperSymbol(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6OutputPuts("xl6_extcall_", ctx->out);
    xl6CgenFuncSymbol(ctx, node);
}

static void xl6CgenExternCallWrapperProto(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t i;

    returns = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenExternCallWrapperSymbol(ctx, node);
    xl6OutputPutc('(', ctx->out);

    params = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("void", ctx->out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];
            if ( i > 0 ) {
                xl6OutputPuts(", ", ctx->out);
            }
            xl6OutputPuts(xl6CgenCAbiTypeName(ctx, param->children[0]), ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, param->text);
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenExternCallWrapper(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* params;
    size_t i;

    if ( !xl6CgenExternCallNeedsWrapper(ctx, node) ) {
        return;
    }
    returns = node->childCount > 2 ? node->children[2] : NULL;
    params = node->childCount > 1 ? node->children[1] : NULL;

    xl6CgenExternCallWrapperProto(ctx, node);
    xl6OutputPuts("\n{\n", ctx->out);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
        xl6OutputPuts(" result = ", ctx->out);
    } else {
        xl6OutputPuts("    ", ctx->out);
    }
    xl6CgenCStringSymbol(ctx->out, node->text);
    xl6OutputPutc('(', ctx->out);
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        xl6CgenFputsCIdent(ctx->out, params->children[i]->text);
    }
    xl6OutputPuts(");\n", ctx->out);

    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        XL6_AstNode* typeNode;
        int isXValue;

        typeNode = xl6CgenCloneCanonicalTypeNode(ctx, param->children[0]);
        isXValue = xl6CgenIsXValueType(typeNode != NULL ? typeNode : param->children[0]);
        xl6AstFree(typeNode);
        if ( isXValue ) {
            xl6OutputPuts("    if (", ctx->out);
            xl6CgenFputsCIdent(ctx->out, param->text);
            xl6OutputPuts(" != 0) { xrtValueRelease(", ctx->out);
            xl6CgenFputsCIdent(ctx->out, param->text);
            xl6OutputPuts("); }\n", ctx->out);
        }
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    return result;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
}
