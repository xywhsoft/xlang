static void xl6CgenUnionFieldDecl(XL6_CgenContext* ctx, const XL6_AstNode* field, int indent)
{
    int i;

    if ( ctx == NULL || field == NULL || field->kind != XL6_AST_FIELD || field->childCount == 0 ) {
        return;
    }
    for ( i = 0; i < indent; ++i ) {
        xl6OutputPuts("    ", ctx->out);
    }
    xl6OutputPuts(xl6CgenTypeName(ctx, field->children[0]), ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenFputsCIdent(ctx->out, field->text);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenUnionDecls(XL6_CgenContext* ctx, const XL6_AstNode* unionNode);
static void xl6CgenClassDecls(XL6_CgenContext* ctx, const XL6_AstNode* classNode);

static void xl6CgenTypeDeclDependency(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    XL6_AstNode* classNode;
    XL6_AstNode* unionNode;

    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL || typeNode->childCount > 0 ) {
        return;
    }

    classNode = xl6CgenFindClass(ctx, typeNode->text);
    if ( classNode != NULL ) {
        xl6CgenClassDecls(ctx, classNode);
        return;
    }

    unionNode = xl6CgenFindUnion(ctx, typeNode->text);
    if ( unionNode != NULL ) {
        xl6CgenUnionDecls(ctx, unionNode);
    }
}

static void xl6CgenInlineUnionDeclDependencies(XL6_CgenContext* ctx, const XL6_AstNode* unionNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( ctx == NULL || unionNode == NULL ) {
        return;
    }
    body = xl6CgenUnionBody(unionNode);
    if ( body == NULL ) {
        return;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        if ( field != NULL && field->kind == XL6_AST_FIELD && field->childCount > 0 ) {
            xl6CgenTypeDeclDependency(ctx, field->children[0]);
        }
    }
}

static void xl6CgenUnionDecls(XL6_CgenContext* ctx, const XL6_AstNode* unionNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( ctx == NULL || unionNode == NULL || unionNode->text == NULL ) {
        return;
    }
    if ( xl6CgenUnionWasEmitted(ctx, unionNode->text) ) {
        return;
    }
    xl6CgenMarkUnionEmitted(ctx, unionNode->text);

    body = xl6CgenUnionBody(unionNode);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            const XL6_AstNode* field = body->children[i];
            if ( field != NULL && field->kind == XL6_AST_FIELD && field->childCount > 0 ) {
                xl6CgenTypeDeclDependency(ctx, field->children[0]);
            }
        }
    }

    xl6OutputPuts("typedef union ", ctx->out);
    xl6CgenUnionTypeName(ctx->out, unionNode->text);
    xl6OutputPuts(" {\n", ctx->out);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            xl6CgenUnionFieldDecl(ctx, body->children[i], 1);
        }
    }
    xl6OutputPuts("} ", ctx->out);
    xl6CgenUnionTypeName(ctx->out, unionNode->text);
    xl6OutputPuts(";\n", ctx->out);
    if ( ctx->needsXValueRuntime ) {
        char cTypeName[256];

        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), unionNode->text);
        xl6CgenEmitPlainRecordBoxRuntime(ctx, cTypeName, unionNode->text);
    }
}

static void xl6CgenInlineUnionFieldDecl(XL6_CgenContext* ctx, const XL6_AstNode* unionNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( ctx == NULL || unionNode == NULL ) {
        return;
    }
    xl6OutputPuts("    union {\n", ctx->out);
    body = xl6CgenUnionBody(unionNode);
    if ( body != NULL ) {
        for ( i = 0; i < body->childCount; ++i ) {
            xl6CgenUnionFieldDecl(ctx, body->children[i], 2);
        }
    }
    xl6OutputPuts("    }", ctx->out);
    if ( unionNode->text != NULL ) {
        xl6OutputPutc(' ', ctx->out);
        xl6CgenFputsCIdent(ctx->out, unionNode->text);
    }
    xl6OutputPuts(";\n", ctx->out);
}
