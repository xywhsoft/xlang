static XL6_AstNode* xl6CgenClassDeclFromTypeMemberReceiver(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;

    if ( ctx == NULL || member == NULL || member->kind != XL6_AST_MEMBER || member->childCount == 0 ) {
        return NULL;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME || receiver->text == NULL ) {
        return NULL;
    }
    if ( xl6CgenFindVarType(ctx, receiver->text) != NULL ) {
        return NULL;
    }
    return xl6CgenFindClass(ctx, receiver->text);
}

static const XL6_AstNode* xl6CgenFindStaticClassFieldNode(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* fieldName,
    const XL6_AstNode** outOwnerClass)
{
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    size_t i;

    if ( outOwnerClass != NULL ) {
        *outOwnerClass = NULL;
    }
    if ( ctx == NULL || classNode == NULL || fieldName == NULL ) {
        return NULL;
    }

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_FIELD &&
             xl6CgenIsStaticMember(member) &&
             xl6CTextEquals(member->text, fieldName) ) {
            if ( outOwnerClass != NULL ) {
                *outOwnerClass = classNode;
            }
            return member;
        }
    }

    baseClass = xl6CgenBaseClass(ctx, classNode);
    return baseClass != NULL ?
        xl6CgenFindStaticClassFieldNode(ctx, baseClass, fieldName, outOwnerClass) :
        NULL;
}

static void xl6CgenStaticClassFieldSymbol(XL6_Output* out, const XL6_AstNode* classNode, const XL6_AstNode* field)
{
    xl6OutputPuts("xl_class_", out);
    xl6CgenFputsEncodedText(out, classNode != NULL ? classNode->text : "_");
    xl6OutputPuts("____static__", out);
    xl6CgenFputsEncodedText(out, field != NULL ? field->text : "_");
}

static int xl6CgenStaticClassFieldExpr(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* field;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ) {
        return 0;
    }
    classNode = xl6CgenClassDeclFromTypeMemberReceiver(ctx, member);
    if ( classNode == NULL ) {
        return 0;
    }
    ownerClass = NULL;
    field = xl6CgenFindStaticClassFieldNode(ctx, classNode, member->text, &ownerClass);
    if ( field == NULL ) {
        return 0;
    }
    xl6CgenStaticClassFieldSymbol(ctx->out, ownerClass != NULL ? ownerClass : classNode, field);
    return 1;
}

static XL6_AstNode* xl6CgenCloneStaticClassFieldType(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* field;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ) {
        return NULL;
    }
    classNode = xl6CgenClassDeclFromTypeMemberReceiver(ctx, member);
    if ( classNode == NULL ) {
        return NULL;
    }
    ownerClass = NULL;
    field = xl6CgenFindStaticClassFieldNode(ctx, classNode, member->text, &ownerClass);
    if ( field == NULL || field->childCount == 0 ) {
        return NULL;
    }
    (void)ownerClass;
    return xl6CgenCloneCanonicalTypeNode(ctx, field->children[0]);
}

static int xl6CgenStaticClassMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* args;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL ) {
        return 0;
    }
    classNode = xl6CgenClassDeclFromTypeMemberReceiver(ctx, member);
    if ( classNode == NULL ) {
        return 0;
    }

    ownerClass = NULL;
    method = xl6CgenResolveStaticClassFunc(ctx, classNode, call, member->text, &ownerClass);
    if ( method == NULL ) {
        return 0;
    }
    xl6CgenClassCallableSymbol(ctx, ownerClass != NULL ? ownerClass : classNode, method);
    xl6OutputPutc('(', ctx->out);
    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    xl6CgenKnownCallArgs(ctx, method, args);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static void xl6CgenStaticClassFieldDecls(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* typeNode;
        const XL6_AstNode* initExpr;

        if ( member == NULL || member->kind != XL6_AST_FIELD || !xl6CgenIsStaticMember(member) || member->childCount == 0 ) {
            continue;
        }
        typeNode = member->children[0];
        initExpr = xl6CgenFieldDefaultExpr(member);
        xl6OutputPuts("static ", ctx->out);
        xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
        xl6OutputPutc(' ', ctx->out);
        xl6CgenStaticClassFieldSymbol(ctx->out, classNode, member);
        xl6OutputPuts(" = ", ctx->out);
        if ( initExpr != NULL ) {
            xl6CgenExpr(ctx, initExpr);
        } else {
            xl6CgenDefaultValue(ctx, typeNode);
        }
        xl6OutputPuts(";\n", ctx->out);
    }
}
