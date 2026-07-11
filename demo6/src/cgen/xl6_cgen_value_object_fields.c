/*
    class、ref<class> 与 union 的成员字段访问集中在此分片。
    本文件由 xl6_cgen_value_object.c 在原有声明位置包含，以保持静态函数可见性与生成顺序不变。
*/

static int xl6CgenClassHasDirectField(const XL6_AstNode* classNode, const char* fieldName)
{
    return xl6CgenFindDirectClassFieldNode(classNode, fieldName) != NULL;
}

static int xl6CgenEmitClassFieldSuffixWithSep(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* fieldName, const char* sep)
{
    const XL6_AstNode* base;
    XL6_AstNode* baseClass;

    if ( classNode == NULL || fieldName == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasDirectField(classNode, fieldName) ) {
        xl6OutputPuts(sep != NULL ? sep : ".", ctx->out);
        xl6CgenFputsCIdent(ctx->out, fieldName);
        return 1;
    }

    base = classNode->childCount > 0 ? classNode->children[0] : NULL;
    if ( base == NULL || base->kind != XL6_AST_NAME ) {
        return 0;
    }
    baseClass = xl6CgenFindClass(ctx, base->text);
    if ( baseClass == NULL ) {
        return 0;
    }

    xl6OutputPuts(sep != NULL ? sep : ".", ctx->out);
    xl6OutputPuts("base", ctx->out);
    return xl6CgenEmitClassFieldSuffixWithSep(ctx, baseClass, fieldName, ".");
}

static int xl6CgenEmitClassFieldSuffix(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* fieldName)
{
    return xl6CgenEmitClassFieldSuffixWithSep(ctx, classNode, fieldName, ".");
}

static const XL6_AstNode* xl6CgenMemberFieldType(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* field;
    XL6_AstNode* classNode;
    XL6_AstNode* ownedReceiverType;

    if ( member == NULL || member->kind != XL6_AST_MEMBER ) {
        return NULL;
    }

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME &&
                              receiver->kind != XL6_AST_MEMBER &&
                              receiver->kind != XL6_AST_INDEX) ) {
        return NULL;
    }

    receiverType = NULL;
    ownedReceiverType = NULL;
    classNode = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX ) {
        ownedReceiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    } else if ( xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        classNode = (XL6_AstNode*)ctx->currentClass;
    } else {
        ownedReceiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        classNode = receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL;
    }

    if ( receiverType != NULL && xl6CgenIsUnionType(ctx, receiverType) ) {
        field = xl6CgenFindUnionFieldNode(xl6CgenFindUnion(ctx, receiverType->text), member->text);
        xl6AstFree(ownedReceiverType);
        return field != NULL && field->childCount > 0 ? field->children[0] : NULL;
    }
    if ( classNode == NULL ) {
        xl6AstFree(ownedReceiverType);
        return NULL;
    }

    field = xl6CgenFindClassFieldNode(ctx, classNode, member->text);
    xl6AstFree(ownedReceiverType);
    return field != NULL && field->childCount > 0 ? field->children[0] : NULL;
}

static int xl6CgenUnionFieldAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* unionNode;
    const XL6_AstNode* field;
    XL6_AstNode* ownedReceiverType;

    if ( member == NULL || member->kind != XL6_AST_MEMBER || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME &&
                              receiver->kind != XL6_AST_MEMBER &&
                              receiver->kind != XL6_AST_INDEX) ) {
        return 0;
    }

    ownedReceiverType = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX ) {
        ownedReceiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
    } else {
        receiverType = xl6CgenFindVarType(ctx, receiver->text);
    }
    if ( !xl6CgenIsUnionType(ctx, receiverType) ) {
        xl6AstFree(ownedReceiverType);
        return 0;
    }

    unionNode = xl6CgenFindUnion(ctx, receiverType->text);
    field = xl6CgenFindUnionFieldNode(unionNode, member->text);
    if ( field == NULL ) {
        xl6AstFree(ownedReceiverType);
        return 0;
    }

    if ( receiver->kind == XL6_AST_NAME ) {
        xl6CgenNamedValueReference(ctx, receiver->text);
    } else {
        xl6CgenExpr(ctx, receiver);
    }
    xl6OutputPutc('.', ctx->out);
    xl6CgenFputsCIdent(ctx->out, member->text);
    xl6AstFree(ownedReceiverType);
    return 1;
}

static void xl6CgenClassFieldAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    XL6_AstNode* classNode;
    XL6_AstNode* ownedReceiverType;

    receiver = member->childCount > 0 ? member->children[0] : NULL;
    if ( receiver == NULL || (receiver->kind != XL6_AST_NAME &&
                              receiver->kind != XL6_AST_MEMBER &&
                              receiver->kind != XL6_AST_INDEX &&
                              receiver->kind != XL6_AST_CALL) ) {
        xl6CgenError(ctx, member, "class field receiver must be name, member, index or call");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    if ( receiver->kind == XL6_AST_NAME && xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        if ( xl6CgenFindClassFieldNode(ctx, ctx->currentClass, member->text) == NULL &&
             xl6CgenClassPropertyMemberAccess(ctx, member) ) {
            return;
        }
        if ( xl6CgenFindClassFieldNode(ctx, ctx->currentClass, member->text) == NULL &&
             xl6CgenClassProtocolMemberAccess(ctx, member) ) {
            return;
        }
        xl6OutputPuts("self", ctx->out);
        if ( !xl6CgenEmitClassFieldSuffixWithSep(ctx, ctx->currentClass, member->text, "->") ) {
            xl6CgenError(ctx, member, "class field not found");
        }
        return;
    }

    ownedReceiverType = NULL;
    if ( receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_INDEX || receiver->kind == XL6_AST_CALL ) {
        const XL6_AstNode* refTarget;
        ownedReceiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        refTarget = xl6CgenRefTargetType(ctx, receiverType);
        classNode = refTarget != NULL ? xl6CgenFindClass(ctx, refTarget->text) :
                    (receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL);
        if ( classNode == NULL ) {
            xl6AstFree(ownedReceiverType);
            xl6CgenError(ctx, member, "class field receiver is not a class value");
            xl6OutputPuts("0", ctx->out);
            return;
        }

        if ( xl6CgenFindClassFieldNode(ctx, classNode, member->text) == NULL ) {
            if ( xl6CgenClassPropertyMemberAccess(ctx, member) ||
                 xl6CgenClassProtocolMemberAccess(ctx, member) ) {
                xl6AstFree(ownedReceiverType);
                return;
            }
        }
        if ( refTarget != NULL ) {
            xl6OutputPrintf(ctx->out, "(*((%s*)xl6_ref_data(", xl6CgenTypeName(ctx, refTarget));
            xl6CgenExpr(ctx, receiver);
            xl6OutputPrintf(ctx->out, ", \"\", %d, %d)))", receiver->line, receiver->column);
        } else {
            xl6CgenExpr(ctx, receiver);
        }
        if ( !xl6CgenEmitClassFieldSuffix(ctx, classNode, member->text) ) {
            xl6CgenError(ctx, member, "class field not found");
        }
        xl6AstFree(ownedReceiverType);
        return;
    }

    ownedReceiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    receiverType = ownedReceiverType;
    {
        const XL6_AstNode* refTarget = xl6CgenRefTargetType(ctx, receiverType);
        classNode = refTarget != NULL ? xl6CgenFindClass(ctx, refTarget->text) :
                    (receiverType != NULL ? xl6CgenFindClass(ctx, receiverType->text) : NULL);
    }
    if ( classNode == NULL ) {
        xl6AstFree(ownedReceiverType);
        xl6CgenError(ctx, member, "class field receiver is not a class value");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    if ( xl6CgenFindClassFieldNode(ctx, classNode, member->text) == NULL ) {
        if ( xl6CgenClassPropertyMemberAccess(ctx, member) ||
             xl6CgenClassProtocolMemberAccess(ctx, member) ) {
            xl6AstFree(ownedReceiverType);
            return;
        }
    }
    if ( xl6CgenRefTargetType(ctx, receiverType) != NULL ) {
        const XL6_AstNode* refTarget = xl6CgenRefTargetType(ctx, receiverType);
        xl6OutputPrintf(ctx->out, "(*((%s*)xl6_ref_data(", xl6CgenTypeName(ctx, refTarget));
        xl6CgenNamedValueReference(ctx, receiver->text);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d)))", receiver->line, receiver->column);
    } else {
        xl6CgenNamedValueReference(ctx, receiver->text);
    }
    if ( !xl6CgenEmitClassFieldSuffix(ctx, classNode, member->text) ) {
        xl6CgenError(ctx, member, "class field not found");
    }
    xl6AstFree(ownedReceiverType);
}
