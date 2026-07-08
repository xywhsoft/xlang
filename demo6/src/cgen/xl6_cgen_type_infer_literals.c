static XL6_AstNode* xl6CgenCloneCommonLiteralElementType(XL6_CgenContext* ctx, const XL6_AstNode* literal, int usePairValue)
{
    XL6_AstNode* common;
    size_t i;

    if ( literal == NULL || literal->childCount == 0 ) {
        return NULL;
    }

    common = NULL;
    for ( i = 0; i < literal->childCount; ++i ) {
        const XL6_AstNode* item = literal->children[i];
        const XL6_AstNode* value = usePairValue ?
            (item != NULL && item->childCount > 1 ? item->children[1] : NULL) :
            item;
        XL6_AstNode* valueType = xl6CgenCloneExprTypeForInference(ctx, value);
        XL6_AstNode* canonicalType;

        if ( valueType == NULL ) {
            xl6AstFree(common);
            return NULL;
        }
        canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, valueType);
        xl6AstFree(valueType);
        if ( canonicalType == NULL ) {
            xl6AstFree(common);
            return NULL;
        }

        if ( common == NULL ) {
            common = canonicalType;
        } else if ( xl6CgenTypeNodeEqual(common, canonicalType) ) {
            xl6AstFree(canonicalType);
        } else {
            xl6AstFree(common);
            xl6AstFree(canonicalType);
            return NULL;
        }
    }
    return common;
}

static const char* xl6CgenContainerLiteralName(const XL6_AstNode* literal)
{
    if ( literal == NULL ) {
        return NULL;
    }
    switch ( literal->kind ) {
    case XL6_AST_ARRAY_LITERAL:
        return "array";
    case XL6_AST_LIST_LITERAL:
        return "list";
    case XL6_AST_SET_LITERAL:
        return "set";
    case XL6_AST_DICT_LITERAL:
        return "dict";
    default:
        return NULL;
    }
}

static XL6_AstNode* xl6CgenCloneContainerLiteralType(
    XL6_CgenContext* ctx,
    const char* forcedName,
    const XL6_AstNode* literal,
    const XL6_AstNode* at)
{
    const char* name;
    int usePairValue;
    XL6_AstNode* elementType;

    name = forcedName != NULL ? forcedName : xl6CgenContainerLiteralName(literal);
    if ( name == NULL ) {
        return NULL;
    }

    if ( literal == NULL || literal->childCount == 0 ) {
        return xl6CgenNewSimpleType(name, at);
    }

    usePairValue = xl6CTextEquals(name, "list") || xl6CTextEquals(name, "dict");
    elementType = xl6CgenCloneCommonLiteralElementType(ctx, literal, usePairValue);
    if ( elementType == NULL ) {
        return xl6CgenNewSimpleType(name, at);
    }
    if ( xl6CTextEquals(elementType->text, "null") ) {
        xl6AstFree(elementType);
        elementType = xl6CgenNewSimpleType("var", at);
    }
    return xl6CgenNewGenericTypeNode(name, elementType, at);
}
