static void xl6CgenXValueMemberKeyExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* key)
{
    xl6OutputPuts("xl6_record_field_get_owned(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, key);
    xl6OutputPuts(xl6CgenExprIsXValueBorrow(ctx, receiver) ? ", 0" : ", 1", ctx->out);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenXValueMemberLengthExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* receiverType)
{
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) && xl6CgenIsTypedContainerType(ctx, receiverType) ) {
        const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, receiverType);
        const char* typeName = concreteType != NULL ? concreteType->text : NULL;

        /* XRT 计数使用无符号类型，xlang length 的稳定类型是 int64。 */
        xl6OutputPuts("((long long)", ctx->out);
        if ( xl6CTextEquals(typeName, "array") ) {
            xl6OutputPuts("xl6TypedArrayCount(", ctx->out);
        } else if ( xl6CTextEquals(typeName, "list") ) {
            xl6OutputPuts("xl6TypedListCount(", ctx->out);
        } else if ( xl6CTextEquals(typeName, "set") ) {
            xl6OutputPuts("xl6TypedSetCount(", ctx->out);
        } else if ( xl6CTextEquals(typeName, "dict") ) {
            xl6OutputPuts("xl6TypedDictCount(", ctx->out);
        } else {
            xl6CgenError(ctx, receiver, "typed container length requires array/list/set/dict");
            xl6OutputPuts("0", ctx->out);
            return;
        }
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts("))", ctx->out);
        return;
	}

	if ( xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
		if ( receiver != NULL && receiver->kind == XL6_AST_NAME &&
			 (xl6CgenIsArrayXValueType(receiverType) ||
			  xl6CgenIsListXValueType(receiverType) ||
			  xl6CgenIsSetXValueType(receiverType) ||
			  xl6CgenIsDictXValueType(receiverType)) ) {
			/* XRT 计数使用 size_t，xlang length 的稳定类型是 int64。 */
			xl6OutputPuts("((long long)xrtValueCount(", ctx->out);
			xl6CgenNamedValueReference(ctx, receiver->text);
			xl6OutputPuts("))", ctx->out);
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

static int xl6CgenTypedContainerMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    if ( !xl6CTextEquals(member->text, "length") ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenExprUsesTypedContainerStorage(ctx, receiver) ||
         !xl6CgenIsTypedContainerType(ctx, receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    xl6CgenXValueMemberLengthExpr(ctx, receiver, receiverType);
    xl6AstFree(receiverType);
    return 1;
}

/*
 * string.length 与 string.len 保持同一口径：返回 UTF-8 codepoint 数量。
 * 直接使用 XRT，避免在生成代码中维护第二套 UTF-8 扫描实现。
 */
static int xl6CgenStaticStringMemberAccess(
    XL6_CgenContext* ctx,
    const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ||
         !xl6CTextEquals(member->text, "length") ) {
        return 0;
    }
    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsStringTypeNode(ctx, receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    xl6OutputPuts("((long long)xrtUtf8Count(xrtStrView(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(")))", ctx->out);
    xl6AstFree(receiverType);
    return 1;
}

static int xl6CgenSafeStringMemberAccess(
    XL6_CgenContext* ctx,
    const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* effectiveType;
    XL6_AstNode* receiverType;
    XL6_AstNode* resultType;
    XL6_CgenCoalesceSlot* slot;
    char slotName[64];
    char resultCType[64];

    if ( member == NULL || member->text == NULL || member->childCount == 0 ||
         !xl6CTextEquals(member->text, "length") ) {
        return 0;
    }
    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    effectiveType = xl6CgenIsNullableType(receiverType)
        ? xl6CgenNullableInnerType(receiverType)
        : receiverType;
    if ( !xl6CgenIsStringTypeNode(ctx, effectiveType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    resultType = xl6CgenCloneExprTypeForInference(ctx, member);
    slot = xl6CgenFindCoalesceSlot(ctx, member);
    if ( resultType == NULL || !xl6CgenNullableUsesTaggedValue(resultType) ||
         slot == NULL ) {
        xl6CgenError(ctx, member, "safe string length requires a prepared nullable result");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(resultType);
        xl6AstFree(receiverType);
        return 1;
    }

    xl6CgenCoalesceSlotName(slotName, sizeof(slotName), slot);
    xl6CgenNullableCTypeName(resultCType, sizeof(resultCType), resultType);
    xl6OutputPrintf(ctx->out, "((%s = ", slotName);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPrintf(
        ctx->out,
        "), %s != 0 ? (%s){ true, (long long)xrtUtf8Count(xrtStrView(%s)) } : (%s){0})",
        slotName,
        resultCType,
        slotName,
        resultCType);

    xl6AstFree(resultType);
    xl6AstFree(receiverType);
    return 1;
}

static int xl6CgenSafeTypedContainerMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* effectiveType;
    const XL6_AstNode* concreteType;
    const char* typeName;
    XL6_AstNode* receiverType;
    XL6_AstNode* resultType;
    XL6_CgenCoalesceSlot* slot;
    char slotName[64];
    char resultCType[64];

    if ( member == NULL || member->text == NULL || member->childCount == 0 ||
         !xl6CTextEquals(member->text, "length") ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    effectiveType = xl6CgenIsNullableType(receiverType)
        ? xl6CgenNullableInnerType(receiverType)
        : receiverType;
    if ( !xl6CgenIsTypedContainerType(ctx, effectiveType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    resultType = xl6CgenCloneExprTypeForInference(ctx, member);
    slot = xl6CgenFindCoalesceSlot(ctx, member);
    if ( resultType == NULL || !xl6CgenNullableUsesTaggedValue(resultType) || slot == NULL ) {
        xl6CgenError(ctx, member, "typed safe length requires a prepared nullable result");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(resultType);
        xl6AstFree(receiverType);
        return 1;
    }

    concreteType = xl6CgenTypedContainerConcreteType(ctx, effectiveType);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    xl6CgenCoalesceSlotName(slotName, sizeof(slotName), slot);
    xl6CgenNullableCTypeName(resultCType, sizeof(resultCType), resultType);

    xl6OutputPrintf(ctx->out, "((%s = ", slotName);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPrintf(ctx->out, "), %s != 0 ? (%s){ true, (long long)", slotName, resultCType);
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6TypedArrayCount(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6TypedListCount(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6TypedSetCount(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6TypedDictCount(", ctx->out);
    } else {
        xl6CgenError(ctx, member, "typed safe length requires array/list/set/dict");
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPrintf(
        ctx->out,
        "%s) } : (%s){0})",
        slotName,
        resultCType);
    xl6AstFree(resultType);
    xl6AstFree(receiverType);
    return 1;
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
    if ( xl6CgenClassHandleTargetType(ctx, receiverType) != NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }
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
        xl6OutputPuts("xrtValueNull()", ctx->out);
    }
    xl6AstFree(receiverType);
    return 1;
}

static int xl6CgenErrorMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;

    if ( member == NULL || member->text == NULL || member->childCount == 0 ) {
        return 0;
    }
    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsErrorType(ctx, receiverType) ) {
        xl6AstFree(receiverType);
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
    } else if ( xl6CTextEquals(member->text, "domain") ) {
        xl6OutputPuts("xl6_error_domain(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "operation") ) {
        xl6OutputPuts("xl6_error_operation(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "data") ) {
        xl6OutputPuts("xl6_error_data(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "code") ) {
        xl6OutputPuts("xl6_error_code(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "systemCode") ) {
        xl6OutputPuts("xl6_error_system_code(", ctx->out);
    } else if ( xl6CTextEquals(member->text, "cause") ) {
        xl6OutputPuts("xl6_error_cause(", ctx->out);
    } else {
        xl6AstFree(receiverType);
        return 0;
    }
    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(
        xl6CgenExprIsManagedBorrow(ctx, receiver) ? ", 0)" : ", 1)",
        ctx->out);
    xl6AstFree(receiverType);
    return 1;
}

static int xl6CgenSafeXValueMemberAccess(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* resultType;
    char fromXValueName[64];
    int taggedResult;

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
            xl6OutputPuts("xrtValueNull()", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    resultType = xl6CgenCloneExprTypeForInference(ctx, member);
    taggedResult = xl6CgenNullableUsesTaggedValue(resultType);
    if ( taggedResult ) {
        xl6CgenNullableFromXValueHelperName(
            fromXValueName,
            sizeof(fromXValueName),
            resultType);
        xl6OutputPrintf(ctx->out, "%s(", fromXValueName);
    }

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) &&
         xl6CgenIsTypedContainerType(ctx, receiverType) ) {
        xl6OutputPuts("xrtValueInt((long long)(", ctx->out);
        xl6CgenXValueMemberLengthExpr(ctx, receiver, receiverType);
        xl6OutputPuts("))", ctx->out);
        if ( taggedResult ) {
            xl6OutputPutc(')', ctx->out);
        }
        xl6AstFree(resultType);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( !xl6CgenExprIsXValueBorrow(ctx, receiver) ) {
        xl6OutputPuts("xl6_xrt_member_safe_length_release(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        if ( taggedResult ) {
            xl6OutputPutc(')', ctx->out);
        }
        xl6AstFree(resultType);
        xl6AstFree(receiverType);
        return 1;
    }

    xl6OutputPuts("xl6_xrt_member_safe_length(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPutc(')', ctx->out);
    if ( taggedResult ) {
        xl6OutputPutc(')', ctx->out);
    }
    xl6AstFree(resultType);
    xl6AstFree(receiverType);
    return 1;
}
