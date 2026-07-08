static void xl6CgenXValueMemberKeyExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* key)
{
    if ( xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
        xl6OutputPuts("xl6_xrt_index_safe_get_key(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenCStringLiteral(ctx->out, key);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    xl6OutputPuts("xl6_xrt_index_safe_get_key_retain_release(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, key);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenXValueMemberLengthExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* receiverType)
{
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) && xl6CgenIsTypedContainerType(ctx, receiverType) ) {
        const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, receiverType);
        const char* typeName = concreteType != NULL ? concreteType->text : NULL;

        if ( xl6CTextEquals(typeName, "array") ) {
            xl6OutputPuts("xrtTypedArrayCount(", ctx->out);
        } else if ( xl6CTextEquals(typeName, "list") ) {
            xl6OutputPuts("xrtTypedListCount(", ctx->out);
        } else if ( xl6CTextEquals(typeName, "set") ) {
            xl6OutputPuts("xrtTypedSetCount(", ctx->out);
        } else if ( xl6CTextEquals(typeName, "dict") ) {
            xl6OutputPuts("xrtTypedDictCount(", ctx->out);
        } else {
            xl6CgenError(ctx, receiver, "typed container length requires array/list/set/dict");
            xl6OutputPuts("0", ctx->out);
            return;
        }
        xl6CgenExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    if ( xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
        if ( receiver != NULL && receiver->kind == XL6_AST_NAME && xl6CgenIsArrayXValueType(receiverType) ) {
            xl6OutputPuts("xvoArrayItemCount(", ctx->out);
            xl6CgenNamedValueReference(ctx, receiver->text);
            xl6OutputPutc(')', ctx->out);
        } else if ( receiver != NULL && receiver->kind == XL6_AST_NAME && xl6CgenIsListXValueType(receiverType) ) {
            xl6OutputPuts("xvoListItemCount(", ctx->out);
            xl6CgenNamedValueReference(ctx, receiver->text);
            xl6OutputPutc(')', ctx->out);
        } else if ( receiver != NULL && receiver->kind == XL6_AST_NAME && xl6CgenIsSetXValueType(receiverType) ) {
            xl6OutputPuts("xvoSetItemCount(", ctx->out);
            xl6CgenNamedValueReference(ctx, receiver->text);
            xl6OutputPutc(')', ctx->out);
        } else if ( receiver != NULL && receiver->kind == XL6_AST_NAME && xl6CgenIsDictXValueType(receiverType) ) {
            xl6OutputPuts("xvoTableItemCount(", ctx->out);
            xl6CgenNamedValueReference(ctx, receiver->text);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_member_length(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
        }
        return;
    }

    xl6OutputPuts("xl6_xrt_member_length_release(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenXValueMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsXValueType(receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    if ( xl6CTextEquals(member->text, "length") ) {
        xl6CgenXValueMemberLengthExpr(ctx, receiver, receiverType);
    } else if ( xl6CgenMemberKeyReceiverIsDynamic(ctx, receiverType) ) {
        xl6CgenXValueMemberKeyExpr(ctx, receiver, member->text);
    } else {
        xl6CgenError(ctx, member, "member key receiver must be var or dynamic dict");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    }
    xl6AstFree(receiverType);
    return 1;
}

static int xl6CgenErrorMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_NAME ) {
        return 0;
    }
    receiverType = xl6CgenFindVarType(ctx, receiver->text);
    if ( receiverType == NULL || !xl6CTextEquals(receiverType->text, "error") ) {
        return 0;
    }

    if ( xl6CTextEquals(member->text, "kind") ) {
        xl6OutputPuts("xl6_error_kind(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "message") ) {
        xl6OutputPuts("xl6_error_message(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "file") ) {
        xl6OutputPuts("xl6_error_file(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "line") ) {
        xl6OutputPuts("xl6_error_line(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "column") ) {
        xl6OutputPuts("xl6_error_column(", ctx->out);
    } else {
        return 0;
    }
    xl6CgenNamedValueReference(ctx, receiver->text);
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenSafeXValueMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsXValueType(receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    if ( !xl6CTextEquals(member->text, "length") ) {
        if ( xl6CgenMemberKeyReceiverIsDynamic(ctx, receiverType) ) {
            xl6CgenXValueMemberKeyExpr(ctx, receiver, member->text);
        } else {
            xl6CgenError(ctx, member, "member key receiver must be var or dynamic dict");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) &&
         xl6CgenIsTypedContainerType(ctx, receiverType) ) {
        xl6OutputPuts("xvoCreateInt((long long)(", ctx->out);
        xl6CgenXValueMemberLengthExpr(ctx, receiver, receiverType);
        xl6OutputPuts("))", ctx->out);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( !xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
        xl6OutputPuts("xl6_xrt_member_safe_length_release(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(receiverType);
        return 1;
    }

    xl6OutputPuts("xl6_xrt_member_safe_length(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPutc(')', ctx->out);
    xl6AstFree(receiverType);
    return 1;
}
