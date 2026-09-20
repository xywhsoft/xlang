static int xl6CgenIsBuiltinObjectClass(const XL6_AstNode* node)
{
    return node != NULL &&
           node->kind == XL6_AST_CLASS &&
           node->text != NULL &&
           xl6CTextEquals(node->text, "object");
}

static int xl6CgenHasBuiltinDynamicFieldsClass(const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* child = root->children[i];

        if ( child != NULL && child->kind == XL6_AST_CLASS &&
             child->text != NULL && xl6CTextEquals(child->text, "DynamicFields") ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenNodeNeedsBuiltinObjectClass(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenIsBuiltinObjectClass(node) ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE &&
         node->text != NULL &&
         xl6CTextEquals(node->text, "object") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CLASS &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "object") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6CTextEquals(node->children[0]->text, "object") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsBuiltinObjectClass(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenIsCopyBuiltinName(const char* name)
{
    return xl6CTextEquals(name, "copy") || xl6CTextEquals(name, "clone");
}

static int xl6CgenCopyBuiltinNameIsShadowed(XL6_CgenContext* ctx, const char* name)
{
    if ( ctx == NULL || name == NULL ) {
        return 0;
    }
    if ( xl6CgenFindVarType(ctx, name) != NULL ) {
        return 1;
    }
    return xl6CgenFindFunc(ctx, name) != NULL;
}
