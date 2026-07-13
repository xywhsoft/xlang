/*
    struct 是显式值语义记录，布局直接对应 C struct。
    它不生成 class 生命周期、虚表或句柄包装代码。
*/
static void xl6CgenStructDecls(XL6_CgenContext* ctx, const XL6_AstNode* structNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( ctx == NULL || structNode == NULL || structNode->text == NULL ||
         xl6CgenStructWasEmitted(ctx, structNode->text) ) {
        return;
    }
    xl6CgenMarkStructEmitted(ctx, structNode->text);
    body = xl6CgenStructBody(structNode);

    /* 按值字段要求依赖类型先完整定义。 */
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_FIELD && member->childCount > 0 ) {
            xl6CgenTypeDeclDependency(ctx, member->children[0]);
        } else if ( member != NULL && member->kind == XL6_AST_UNION ) {
            xl6CgenInlineUnionDeclDependencies(ctx, member);
        }
    }

    xl6OutputPuts("typedef struct ", ctx->out);
    xl6CgenStructTypeName(ctx->out, structNode->text);
    xl6OutputPuts(" {\n", ctx->out);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_FIELD ) {
            xl6CgenUnionFieldDecl(ctx, member, 1);
        } else if ( member != NULL && member->kind == XL6_AST_UNION ) {
            xl6CgenInlineUnionFieldDecl(ctx, member);
        }
    }
    xl6OutputPuts("} ", ctx->out);
    xl6CgenStructTypeName(ctx->out, structNode->text);
    xl6OutputPuts(";\n", ctx->out);

    if ( ctx->needsXValueRuntime ) {
        char cTypeName[256];
        xl6CgenStructTypeTextName(cTypeName, sizeof(cTypeName), structNode->text);
        xl6CgenEmitPlainRecordBoxRuntime(ctx, cTypeName, structNode->text);
    }
}
