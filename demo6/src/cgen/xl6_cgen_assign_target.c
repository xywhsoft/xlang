static int xl6CgenSimpleAssignTargetPath(const XL6_AstNode* left, char* buffer, size_t capacity)
{
    char receiver[256];
    char field[256];

    if ( left == NULL || buffer == NULL || capacity == 0 ) {
        return 0;
    }
    if ( left->kind == XL6_AST_NAME ) {
        xl6CgenCIdentText(buffer, capacity, left->text);
        return 1;
    }
    if ( left->kind != XL6_AST_MEMBER || left->childCount == 0 || left->children[0] == NULL ||
         left->children[0]->kind != XL6_AST_NAME ) {
        return 0;
    }

    xl6CgenCIdentText(field, sizeof(field), left->text);
    if ( xl6CTextEquals(left->children[0]->text, "self") ) {
        snprintf(buffer, capacity, "self->%s", field);
        return 1;
    }

    xl6CgenCIdentText(receiver, sizeof(receiver), left->children[0]->text);
    snprintf(buffer, capacity, "%s.%s", receiver, field);
    return 1;
}

static int xl6CgenTryClassCtorAssignTarget(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const XL6_AstNode* right,
    const XL6_AstNode* targetType)
{
    XL6_AstNode* ctorClass = NULL;
    XL6_AstNode* targetClass;
    char targetPath[512];

    if ( targetType == NULL || !xl6CgenIsClassType(ctx, targetType) ) {
        return 0;
    }
    targetClass = xl6CgenFindClass(ctx, targetType->text);
    if ( !xl6CgenCallIsClassCtor(ctx, right, &ctorClass) ||
         targetClass == NULL ||
         !xl6CTextEquals(targetClass->text, ctorClass->text) ||
         !xl6CgenSimpleAssignTargetPath(left, targetPath, sizeof(targetPath)) ) {
        return 0;
    }

    xl6CgenEmitClassCtorInto(ctx, right, targetPath);
    return 1;
}
