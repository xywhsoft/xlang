/*
    class、class handle 与 union 的成员字段访问集中在此分片。
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

    if ( receiverType != NULL && xl6CgenIsPlainRecordType(ctx, receiverType) ) {
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

static int xl6CgenPlainRecordFieldAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* receiverType;
    const XL6_AstNode* recordNode;
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
    if ( !xl6CgenIsPlainRecordType(ctx, receiverType) ) {
        xl6AstFree(ownedReceiverType);
        return 0;
    }

    recordNode = xl6CgenFindUnion(ctx, receiverType->text);
    field = xl6CgenFindUnionFieldNode(recordNode, member->text);
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
    if ( receiver == NULL ) {
        xl6CgenError(ctx, member, "class field receiver is missing");
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
    if ( receiver->kind != XL6_AST_NAME ) {
        const XL6_AstNode* refTarget;
        ownedReceiverType = receiver->kind == XL6_AST_MEMBER
            ? xl6CgenCloneMemberFieldTypeForInference(ctx, receiver)
            : xl6CgenCloneExprTypeForInference(ctx, receiver);
        receiverType = ownedReceiverType;
        refTarget = xl6CgenClassHandleTargetType(ctx, receiverType);
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
            xl6OutputPrintf(ctx->out, "(*((%s*)xl6_class_data(", xl6CgenTypeName(ctx, refTarget));
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
        const XL6_AstNode* refTarget = xl6CgenClassHandleTargetType(ctx, receiverType);
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
    if ( xl6CgenClassHandleTargetType(ctx, receiverType) != NULL ) {
        const XL6_AstNode* refTarget = xl6CgenClassHandleTargetType(ctx, receiverType);
        xl6OutputPrintf(ctx->out, "(*((%s*)xl6_class_data(", xl6CgenTypeName(ctx, refTarget));
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

static void xl6CgenSafeClassFieldValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverType,
    const XL6_AstNode* classNode,
    const char* fieldName,
    const char* receiverName,
    const XL6_AstNode* at)
{
    xl6OutputPrintf(
        ctx->out,
        "(*((%s*)xl6_class_data(%s, \"\", %d, %d)))",
        xl6CgenTypeName(ctx, receiverType),
        receiverName,
        at != NULL ? at->line : 0,
        at != NULL ? at->column : 0);
    if ( !xl6CgenEmitClassFieldSuffix(ctx, classNode, fieldName) ) {
        xl6CgenError(ctx, at, "safe class field was not found");
    }
}

/*
 * 静态 class 安全字段访问保留字段的真实类型，只在结果上提升为 T?。
 * 接收者保存在语句局部槽中，保证构造调用和其他有副作用表达式只执行一次。
 */
static int xl6CgenSafeClassFieldAccess(
    XL6_CgenContext* ctx,
    const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* effectiveReceiverType;
    const XL6_AstNode* classType;
    const XL6_AstNode* field;
    XL6_AstNode* receiverType;
    XL6_AstNode* fieldType;
    XL6_AstNode* resultType;
    XL6_AstNode* classNode;
    XL6_CgenCoalesceSlot* slot;
    char slotName[64];
    char resultCType[64];

    if ( ctx == NULL || member == NULL ||
         member->kind != XL6_AST_SAFE_MEMBER ||
         member->text == NULL ||
         member->childCount == 0 ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    effectiveReceiverType = xl6CgenIsNullableType(receiverType)
        ? xl6CgenNullableInnerType(receiverType)
        : receiverType;
    classType = xl6CgenClassHandleTargetType(ctx, effectiveReceiverType);
    classNode = classType != NULL ? xl6CgenFindClass(ctx, classType->text) : NULL;
    field = classNode != NULL
        ? xl6CgenFindClassFieldNode(ctx, classNode, member->text)
        : NULL;
    if ( classNode == NULL || field == NULL || field->childCount == 0 ) {
        xl6AstFree(receiverType);
        return 0;
    }

    fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, member);
    resultType = xl6CgenCloneExprTypeForInference(ctx, member);
    slot = xl6CgenFindCoalesceSlot(ctx, member);
    if ( fieldType == NULL || resultType == NULL || slot == NULL ) {
        xl6CgenError(ctx, member, "safe class field lowering plan is incomplete");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(fieldType);
        xl6AstFree(resultType);
        xl6AstFree(receiverType);
        return 1;
    }

    xl6CgenCoalesceSlotName(slotName, sizeof(slotName), slot);
    xl6OutputPrintf(ctx->out, "((%s = ", slotName);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPrintf(ctx->out, "), !xvoIsNull(%s) ? ", slotName);
    if ( xl6CgenNullableUsesTaggedValue(resultType) &&
         !xl6CgenIsNullableType(fieldType) ) {
        xl6CgenNullableCTypeName(resultCType, sizeof(resultCType), resultType);
        xl6OutputPrintf(ctx->out, "(%s){ true, ", resultCType);
        xl6CgenSafeClassFieldValue(
            ctx,
            classType,
            classNode,
            member->text,
            slotName,
            member);
        xl6OutputPuts(" }", ctx->out);
    } else {
        xl6CgenSafeClassFieldValue(
            ctx,
            classType,
            classNode,
            member->text,
            slotName,
            member);
    }
    xl6OutputPuts(" : ", ctx->out);
    if ( xl6CgenNullableUsesTaggedValue(resultType) ) {
        xl6CgenNullableCTypeName(resultCType, sizeof(resultCType), resultType);
        xl6OutputPrintf(ctx->out, "(%s){0}", resultCType);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPutc(')', ctx->out);

    xl6AstFree(fieldType);
    xl6AstFree(resultType);
    xl6AstFree(receiverType);
    return 1;
}
