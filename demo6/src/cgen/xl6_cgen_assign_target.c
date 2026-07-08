static int xl6CgenSimpleAssignTargetPath(XL6_CgenContext* ctx, const XL6_AstNode* left, char* buffer, size_t capacity)
{
    char receiver[256];
    char field[256];
    int written;

    if ( left == NULL || buffer == NULL || capacity == 0 ) {
        return 0;
    }
    if ( left->kind == XL6_AST_NAME ) {
        if ( xl6CgenImplicitSelfFieldPath(ctx, left->text, buffer, capacity) ) {
            return 1;
        }
        xl6CgenCIdentText(buffer, capacity, left->text);
        return 1;
    }
    if ( left->kind != XL6_AST_MEMBER || left->childCount == 0 || left->children[0] == NULL ) {
        return 0;
    }

    xl6CgenCIdentText(field, sizeof(field), left->text);
    if ( left->children[0]->kind == XL6_AST_NAME ) {
        if ( xl6CTextEquals(left->children[0]->text, "self") ) {
            written = snprintf(buffer, capacity, "self->%s", field);
            return written > 0 && (size_t)written < capacity;
        }

        xl6CgenCIdentText(receiver, sizeof(receiver), left->children[0]->text);
        written = snprintf(buffer, capacity, "%s.%s", receiver, field);
        return written > 0 && (size_t)written < capacity;
    }

    if ( !xl6CgenSimpleAssignTargetPath(ctx, left->children[0], receiver, sizeof(receiver)) ) {
        return 0;
    }
    written = snprintf(buffer, capacity, "%s.%s", receiver, field);
    return written > 0 && (size_t)written < capacity;
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
         !xl6CgenSimpleAssignTargetPath(ctx, left, targetPath, sizeof(targetPath)) ) {
        return 0;
    }

    xl6CgenEmitClassCtorInto(ctx, right, targetPath);
    return 1;
}
