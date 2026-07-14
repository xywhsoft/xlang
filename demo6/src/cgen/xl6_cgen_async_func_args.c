static void xl6CgenAsyncArgStructDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* params = xl6CgenAsyncParams(node);
    size_t i;

    if ( params == NULL || params->childCount == 0 ) {
        return;
    }

    xl6OutputPuts("typedef struct ", ctx->out);
    xl6CgenAsyncArgsStructName(ctx, node);
    xl6OutputPuts(" {\n", ctx->out);
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        xl6OutputPuts("    ", ctx->out);
        if ( param != NULL && param->childCount > 0 ) {
            xl6OutputPuts(xl6CgenAsyncParamCType(ctx, param->children[0]), ctx->out);
        } else {
            xl6OutputPuts("int", ctx->out);
        }
        xl6OutputPutc(' ', ctx->out);
        xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6OutputPuts("} ", ctx->out);
    xl6CgenAsyncArgsStructName(ctx, node);
    xl6OutputPuts(";\n\n", ctx->out);
}
