static XL6_AstNode* xl6CgenFindCustom(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node->kind == XL6_AST_CUSTOM && xl6CTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static const char* xl6CgenLookupCallableName(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_EXPORT_FUNC ||
         node->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL ||
         node->kind == XL6_AST_CUSTOM_EXTERN_FUNC ) {
        const XL6_AstNode* nameNode = node->childCount > 0 ? node->children[0] : NULL;
        return nameNode != NULL ? nameNode->text : NULL;
    }
    return node->text;
}

static XL6_AstNode* xl6CgenFindClass(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node->kind == XL6_AST_CLASS && xl6CTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static XL6_AstNode* xl6CgenFindUnion(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node->kind == XL6_AST_UNION && node->text != NULL && xl6CTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static int xl6CgenIsCustomType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && xl6CgenFindCustom(ctx, typeNode->text) != NULL;
}

static int xl6CgenIsClassType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && xl6CgenFindClass(ctx, typeNode->text) != NULL;
}

static int xl6CgenIsUnionType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && typeNode->childCount == 0 &&
           xl6CgenFindUnion(ctx, typeNode->text) != NULL;
}

static int xl6CgenClassHasTypeParams(const XL6_AstNode* classNode)
{
    const XL6_AstNode* typeParams;

    typeParams = classNode != NULL && classNode->childCount > 2 ? classNode->children[2] : NULL;
    return typeParams != NULL && typeParams->childCount > 0;
}

static int xl6CgenAstHasFlag(const XL6_AstNode* node, unsigned int flag)
{
    return node != NULL && (node->flags & flag) != 0;
}

static int xl6CgenIsExternClassProperty(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_GET || node->kind == XL6_AST_LET) &&
           xl6CgenAstHasFlag(node, XL6_AST_FLAG_EXTERN);
}

static int xl6CgenIsStaticMember(const XL6_AstNode* node)
{
    return xl6CgenAstHasFlag(node, XL6_AST_FLAG_STATIC);
}

static int xl6CgenIsInstanceField(const XL6_AstNode* node)
{
    return node != NULL && node->kind == XL6_AST_FIELD && !xl6CgenIsStaticMember(node);
}

static const XL6_AstNode* xl6CgenFieldDefaultExpr(const XL6_AstNode* field)
{
    size_t i;

    if ( field == NULL || field->kind != XL6_AST_FIELD ) {
        return NULL;
    }

    /*
        字段 child[0] 固定为类型。
        从 child[1] 开始，可能是默认值，也可能是字段注解。
    */
    for ( i = 1; i < field->childCount; ++i ) {
        const XL6_AstNode* child = field->children[i];
        if ( child != NULL && child->kind != XL6_AST_FIELD_ATTR ) {
            return child;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenClassTypeParams(const XL6_AstNode* classNode)
{
    return classNode != NULL && classNode->childCount > 2 ? classNode->children[2] : NULL;
}

static const XL6_AstNode* xl6CgenFuncTypeParams(const XL6_AstNode* funcNode)
{
    return funcNode != NULL && funcNode->childCount > 4 ? funcNode->children[4] : NULL;
}

static int xl6CgenFuncIsGeneric(const XL6_AstNode* funcNode)
{
    const XL6_AstNode* typeParams;

    typeParams = xl6CgenFuncTypeParams(funcNode);
    return typeParams != NULL && typeParams->childCount > 0;
}

static int xl6CgenIsFuncDeclNode(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_FUNC ||
            node->kind == XL6_AST_ASYNC_FUNC ||
            node->kind == XL6_AST_EXPORT_FUNC);
}

static XL6_AstNode* xl6CgenCloneCanonicalTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* node);

static const XL6_AstNode* xl6CgenSubstituteGenericType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( ctx == NULL || typeNode == NULL || typeNode->text == NULL ||
         ctx->currentGenericParams == NULL || ctx->currentGenericArgs == NULL ) {
        return typeNode;
    }
    for ( i = 0; i < ctx->currentGenericParams->childCount && i < ctx->currentGenericArgs->childCount; ++i ) {
        const XL6_AstNode* param = ctx->currentGenericParams->children[i];
        if ( param != NULL && xl6CTextEquals(param->text, typeNode->text) ) {
            return ctx->currentGenericArgs->children[i];
        }
    }
    return typeNode;
}

static const XL6_AstNode* xl6CgenClassBody(const XL6_AstNode* classNode)
{
    return classNode != NULL && classNode->childCount > 1 ? classNode->children[1] : NULL;
}

static int xl6CgenBodyHasDerive(const XL6_AstNode* body, const char* ability)
{
    size_t i;

    if ( body == NULL || ability == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_DERIVE &&
             xl6CTextEquals(member->text, ability) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenClassHasDerive(const XL6_AstNode* classNode, const char* ability)
{
    return xl6CgenBodyHasDerive(xl6CgenClassBody(classNode), ability);
}

static int xl6CgenCustomHasDerive(const XL6_AstNode* customNode, const char* ability)
{
    const XL6_AstNode* body;

    body = customNode != NULL && customNode->childCount > 0 ? customNode->children[0] : NULL;
    return xl6CgenBodyHasDerive(body, ability);
}

static const XL6_AstNode* xl6CgenUnionBody(const XL6_AstNode* unionNode)
{
    return unionNode != NULL && unionNode->childCount > 0 ? unionNode->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenFindUnionFieldNode(const XL6_AstNode* unionNode, const char* fieldName)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenUnionBody(unionNode);
    if ( body == NULL || fieldName == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        if ( field != NULL && field->kind == XL6_AST_FIELD && xl6CTextEquals(field->text, fieldName) ) {
            return field;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindClassFunc(const XL6_AstNode* classNode, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* nameNode;
        if ( member == NULL || member->kind != XL6_AST_FUNC || member->childCount == 0 ) {
            continue;
        }
        nameNode = member->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            return member;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindClassSpecial(const XL6_AstNode* classNode, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_CREATE &&
             xl6CTextEquals(member->text, name) ) {
            return member;
        }
        if ( member != NULL &&
             member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL &&
             xl6CTextEquals(xl6CgenLookupCallableName(member), name) ) {
            return member;
        }
    }
    return NULL;
}

static int xl6CgenClassHasCreate(const XL6_AstNode* classNode)
{
    return xl6CgenFindClassSpecial(classNode, "create") != NULL;
}

static const XL6_AstNode* xl6CgenCallableParams(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }

    /*
        不同声明节点的参数表位置不同，这里统一成“可调用参数表”。
        普通 func / custom extern func: child[1]
        class create: child[0]
        custom extern create/destroy: child[1]
    */
    if ( node->kind == XL6_AST_CREATE ) {
        return node->childCount > 0 ? node->children[0] : NULL;
    }
    return node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6CgenFindClassCreateCallableWithoutArgs(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             (member->kind == XL6_AST_CREATE || member->kind == XL6_AST_CUSTOM_EXTERN_SPECIAL) &&
             xl6CTextEquals(xl6CgenLookupCallableName(member), "create") &&
             xl6CgenParamRequiredCount(xl6CgenCallableParams(member)) == 0 ) {
            return member;
        }
    }
    return NULL;
}

static int xl6CgenClassHasNativeHandle(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* typeNode;

        if ( member == NULL ||
             member->kind != XL6_AST_FIELD ||
             (member->flags & XL6_AST_FLAG_NATIVE) == 0 ||
             (member->flags & XL6_AST_FLAG_STATIC) != 0 ||
             !xl6CTextEquals(member->text, "handle") ||
             member->childCount == 0 ) {
            continue;
        }
        typeNode = member->children[0];
        return typeNode != NULL && xl6CTextEquals(typeNode->text, "point");
    }
    return 0;
}

static int xl6CgenClassIsPureNativeHandle(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;
    int hasHandle;

    if ( !xl6CgenClassHasNativeHandle(classNode) ) {
        return 0;
    }

    body = xl6CgenClassBody(classNode);
    hasHandle = 0;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];

        if ( member == NULL || member->kind != XL6_AST_FIELD || xl6CgenIsStaticMember(member) ) {
            continue;
        }
        if ( (member->flags & XL6_AST_FLAG_NATIVE) != 0 &&
             xl6CTextEquals(member->text, "handle") ) {
            hasHandle = 1;
            continue;
        }

        /*
            只有纯句柄类使用 handle type-desc。
            一旦 class 自己携带额外 xlang 字段，就必须保留普通 record 语义。
        */
        return 0;
    }
    return hasHandle;
}

static const XL6_AstNode* xl6CgenFindCustomMember(const XL6_AstNode* customNode, XL6_AstKind kind, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    if ( customNode == NULL || customNode->childCount == 0 || name == NULL ) {
        return NULL;
    }
    body = customNode->children[0];
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* nameNode;
        if ( member == NULL || member->kind != kind || member->childCount == 0 ) {
            continue;
        }
        nameNode = member->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            return member;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindCustomSpecial(const XL6_AstNode* customNode, const char* name)
{
    return xl6CgenFindCustomMember(customNode, XL6_AST_CUSTOM_EXTERN_SPECIAL, name);
}

static const XL6_AstNode* xl6CgenFindCustomExternFunc(const XL6_AstNode* customNode, const char* name)
{
    return xl6CgenFindCustomMember(customNode, XL6_AST_CUSTOM_EXTERN_FUNC, name);
}

static int xl6CgenIsExternalVarDecl(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_EXTERN_VAR || node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST);
}

static const XL6_AstNode* xl6CgenExternalVarNameNode(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenExternalVarTypeNode(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6CgenExternalVarInitExpr(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 2 ? node->children[2] : NULL;
}

static int xl6CgenIsTypeAliasDecl(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_TYPE_ALIAS || node->kind == XL6_AST_EXTERN_TYPE_ALIAS);
}

static const XL6_AstNode* xl6CgenTypeAliasTargetType(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenFindTypeAlias(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        if ( xl6CgenIsTypeAliasDecl(node) && xl6CTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenResolveAliasType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    int guard;

    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* aliasNode;
        if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL || typeNode->childCount > 0 ) {
            return typeNode;
        }
        aliasNode = xl6CgenFindTypeAlias(ctx, typeNode->text);
        if ( aliasNode == NULL ) {
            return typeNode;
        }
        typeNode = xl6CgenTypeAliasTargetType(aliasNode);
    }
    return typeNode;
}

static const XL6_AstNode* xl6CgenFindExternalVar(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        const XL6_AstNode* nameNode;
        if ( !xl6CgenIsExternalVarDecl(node) ) {
            continue;
        }
        nameNode = xl6CgenExternalVarNameNode(node);
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            return node;
        }
    }
    return NULL;
}
