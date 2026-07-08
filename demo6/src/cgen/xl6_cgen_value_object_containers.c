/* ???????? lowering? */

typedef enum XL6_CgenContainerMethodKind {
    XL6_CGEN_CONTAINER_METHOD_NONE = 0,
    XL6_CGEN_CONTAINER_METHOD_VAR,
    XL6_CGEN_CONTAINER_METHOD_ARRAY,
    XL6_CGEN_CONTAINER_METHOD_LIST,
    XL6_CGEN_CONTAINER_METHOD_SET,
    XL6_CGEN_CONTAINER_METHOD_DICT
} XL6_CgenContainerMethodKind;

static const XL6_AstNode* xl6CgenContainerMethodArg(const XL6_AstNode* call, size_t index)
{
    const XL6_AstNode* args;

    args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
    if ( args == NULL || index >= args->childCount ) {
        return NULL;
    }
    return xl6CgenArgExpr(args->children[index]);
}

static XL6_CgenContainerMethodKind xl6CgenContainerMethodKindFromType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverType,
    int* pTyped)
{
    const XL6_AstNode* typedContainer;

    if ( pTyped != NULL ) {
        *pTyped = 0;
    }
    if ( receiverType == NULL ) {
        return XL6_CGEN_CONTAINER_METHOD_NONE;
    }
    if ( xl6CTextEquals(receiverType->text, "var") ) {
        return XL6_CGEN_CONTAINER_METHOD_VAR;
    }

    typedContainer = xl6CgenTypedContainerConcreteType(ctx, receiverType);
    if ( typedContainer != NULL ) {
        if ( pTyped != NULL ) {
            *pTyped = 1;
        }
        if ( xl6CTextEquals(typedContainer->text, "array") ) {
            return XL6_CGEN_CONTAINER_METHOD_ARRAY;
        }
        if ( xl6CTextEquals(typedContainer->text, "list") ) {
            return XL6_CGEN_CONTAINER_METHOD_LIST;
        }
        if ( xl6CTextEquals(typedContainer->text, "set") ) {
            return XL6_CGEN_CONTAINER_METHOD_SET;
        }
        if ( xl6CTextEquals(typedContainer->text, "dict") ) {
            return XL6_CGEN_CONTAINER_METHOD_DICT;
        }
    }

    if ( xl6CgenIsArrayXValueType(receiverType) ) {
        return XL6_CGEN_CONTAINER_METHOD_ARRAY;
    }
    if ( xl6CgenIsListXValueType(receiverType) ) {
        return XL6_CGEN_CONTAINER_METHOD_LIST;
    }
    if ( xl6CgenIsSetXValueType(receiverType) ) {
        return XL6_CGEN_CONTAINER_METHOD_SET;
    }
    if ( xl6CgenIsDictXValueType(receiverType) ) {
        return XL6_CGEN_CONTAINER_METHOD_DICT;
    }
    return XL6_CGEN_CONTAINER_METHOD_NONE;
}

static int xl6CgenContainerMethodSupported(XL6_CgenContainerMethodKind kind, int typed, const char* name)
{
    if ( name == NULL || kind == XL6_CGEN_CONTAINER_METHOD_NONE ) {
        return 0;
    }
    if ( kind == XL6_CGEN_CONTAINER_METHOD_VAR ) {
        return xl6CTextEquals(name, "push") ||
               xl6CTextEquals(name, "append") ||
               xl6CTextEquals(name, "insert") ||
               xl6CTextEquals(name, "remove") ||
               xl6CTextEquals(name, "removeValue") ||
               xl6CTextEquals(name, "pop") ||
               xl6CTextEquals(name, "take") ||
               xl6CTextEquals(name, "clear") ||
               xl6CTextEquals(name, "reserve") ||
               xl6CTextEquals(name, "swap") ||
               xl6CTextEquals(name, "add") ||
               xl6CTextEquals(name, "contains");
    }
    if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
        if ( typed ) {
            return xl6CTextEquals(name, "push") ||
                   xl6CTextEquals(name, "append") ||
                   xl6CTextEquals(name, "insert") ||
                   xl6CTextEquals(name, "remove") ||
                   xl6CTextEquals(name, "removeValue") ||
                   xl6CTextEquals(name, "pop") ||
                   xl6CTextEquals(name, "take") ||
                   xl6CTextEquals(name, "clear") ||
                   xl6CTextEquals(name, "contains") ||
                   xl6CTextEquals(name, "indexOf") ||
                   xl6CTextEquals(name, "reverse");
        }
        return xl6CTextEquals(name, "push") ||
               xl6CTextEquals(name, "append") ||
               xl6CTextEquals(name, "insert") ||
               xl6CTextEquals(name, "remove") ||
               xl6CTextEquals(name, "removeValue") ||
               xl6CTextEquals(name, "pop") ||
               xl6CTextEquals(name, "take") ||
               xl6CTextEquals(name, "clear") ||
               xl6CTextEquals(name, "reserve") ||
               xl6CTextEquals(name, "swap") ||
               xl6CTextEquals(name, "contains");
    }
    if ( kind == XL6_CGEN_CONTAINER_METHOD_LIST ) {
        if ( typed ) {
            return xl6CTextEquals(name, "remove") ||
                   xl6CTextEquals(name, "take") ||
                   xl6CTextEquals(name, "clear") ||
                   xl6CTextEquals(name, "contains");
        }
        return xl6CTextEquals(name, "remove") ||
               xl6CTextEquals(name, "take") ||
               xl6CTextEquals(name, "clear") ||
               xl6CTextEquals(name, "contains");
    }
    if ( kind == XL6_CGEN_CONTAINER_METHOD_SET ) {
        return xl6CTextEquals(name, "add") ||
               xl6CTextEquals(name, "remove") ||
               xl6CTextEquals(name, "contains") ||
               xl6CTextEquals(name, "clear");
    }
    if ( kind == XL6_CGEN_CONTAINER_METHOD_DICT ) {
        return xl6CTextEquals(name, "remove") ||
               xl6CTextEquals(name, "contains") ||
               xl6CTextEquals(name, "take") ||
               xl6CTextEquals(name, "clear");
    }
    return 0;
}

static void xl6CgenContainerReceiverExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver)
{
    xl6CgenExpr(ctx, receiver);
}

static void xl6CgenContainerValueArgExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    xl6CgenXValueFromExpr(ctx, expr);
}

static void xl6CgenTypedArrayValueCall(
    XL6_CgenContext* ctx,
    const char* helper,
    const XL6_AstNode* receiver,
    const XL6_AstNode* index)
{
    xl6OutputPuts(helper, ctx->out);
    xl6OutputPutc('(', ctx->out);
    xl6CgenContainerReceiverExpr(ctx, receiver);
    if ( index != NULL ) {
        xl6OutputPuts(", (unsigned int)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts(") + 1u", ctx->out);
    }
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenTypedArrayValueToItemExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiverType,
    const char* helper,
    const XL6_AstNode* receiver,
    const XL6_AstNode* index)
{
    const XL6_AstNode* itemType;
    const XL6_AstNode* concreteType;
    const char* typeName;

    itemType = xl6CgenTypedContainerItemType(ctx, receiverType);
    concreteType = itemType != NULL ? xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, itemType)) : NULL;
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( xl6CgenIsIntegerTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)xl6_xrt_value_to_i64_release(", xl6CgenTypeName(ctx, concreteType));
        xl6CgenTypedArrayValueCall(ctx, helper, receiver, index);
        xl6OutputPuts("))", ctx->out);
        return 1;
    }
    if ( xl6CgenIsFloatTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)xl6_xrt_value_to_f64_release(", xl6CgenTypeName(ctx, concreteType));
        xl6CgenTypedArrayValueCall(ctx, helper, receiver, index);
        xl6OutputPuts("))", ctx->out);
        return 1;
    }
    if ( xl6CgenIsBoolTypeName(typeName) ) {
        xl6OutputPrintf(ctx->out, "((%s)xl6_xrt_value_to_bool_release(", xl6CgenTypeName(ctx, concreteType));
        xl6CgenTypedArrayValueCall(ctx, helper, receiver, index);
        xl6OutputPuts("))", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(typeName, "string") ) {
        xl6OutputPuts("xl6_xrt_value_to_string_release(", ctx->out);
        xl6CgenTypedArrayValueCall(ctx, helper, receiver, index);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(typeName, "time") ) {
        xl6OutputPrintf(ctx->out, "((%s)xl6_xrt_value_to_time_release(", xl6CgenTypeName(ctx, concreteType));
        xl6CgenTypedArrayValueCall(ctx, helper, receiver, index);
        xl6OutputPuts("))", ctx->out);
        return 1;
    }
    if ( xl6CTextEquals(typeName, "point") ) {
        xl6OutputPrintf(ctx->out, "((%s)xl6_xrt_value_to_point_release(", xl6CgenTypeName(ctx, concreteType));
        xl6CgenTypedArrayValueCall(ctx, helper, receiver, index);
        xl6OutputPuts("))", ctx->out);
        return 1;
    }
    return 0;
}

static int xl6CgenContainerMethodCall(XL6_CgenContext* ctx, const XL6_AstNode* call, const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* args;
    const XL6_AstNode* arg0;
    const XL6_AstNode* arg1;
    XL6_AstNode* receiverType;
    XL6_CgenContainerMethodKind kind;
    int typed;
    const char* name;
    size_t argCount;

    if ( call == NULL || member == NULL || member->kind != XL6_AST_MEMBER ||
         member->text == NULL || member->childCount == 0 ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    kind = xl6CgenContainerMethodKindFromType(ctx, receiverType, &typed);
    name = member->text;
    if ( !xl6CgenContainerMethodSupported(kind, typed, name) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    args = call->childCount > 1 ? call->children[1] : NULL;
    argCount = args != NULL ? args->childCount : 0;
    arg0 = xl6CgenContainerMethodArg(call, 0);
    arg1 = xl6CgenContainerMethodArg(call, 1);

    if ( (xl6CTextEquals(name, "push") || xl6CTextEquals(name, "append")) &&
         argCount == 1 && arg0 != NULL ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xl6_xrt_typed_array_append_value_release(", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xl6_xrt_array_push_value(", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_VAR ) {
            xl6OutputPuts("xl6_xrt_container_push_value(", ctx->out);
        } else {
            xl6CgenError(ctx, call, "push/append requires array receiver");
            xl6OutputPuts("0", ctx->out);
            xl6AstFree(receiverType);
            return 1;
        }
        xl6CgenContainerReceiverExpr(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenContainerValueArgExpr(ctx, arg0);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "insert") && argCount == 2 && arg0 != NULL && arg1 != NULL ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xl6_xrt_typed_array_insert_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts(") + 1u, ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg1);
            xl6OutputPutc(')', ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY && !typed ) {
            xl6OutputPuts("xl6_xrt_array_insert_value(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("), ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg1);
            xl6OutputPutc(')', ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_VAR ) {
            xl6OutputPuts("xl6_xrt_container_insert_value(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg1);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenError(ctx, call, "insert requires dynamic array receiver");
            xl6OutputPuts("0", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "remove") && (argCount == 1 || argCount == 2) && arg0 != NULL ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xrtTypedArrayRemove(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts(") + 1u, (unsigned int)(", ctx->out);
            if ( argCount == 2 && arg1 != NULL ) {
                xl6CgenExpr(ctx, arg1);
            } else {
                xl6OutputPuts("1", ctx->out);
            }
            xl6OutputPuts("))", ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_LIST ) {
            xl6OutputPuts("xrtTypedListRemove(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("))", ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_SET ) {
            xl6OutputPuts("xl6_xrt_typed_set_remove_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_DICT ) {
            xl6OutputPuts("xrtTypedDictRemove(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts(", 0)", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xvoArrayRemove(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("), (unsigned int)(", ctx->out);
            if ( argCount == 2 && arg1 != NULL ) {
                xl6CgenExpr(ctx, arg1);
            } else {
                xl6OutputPuts("1", ctx->out);
            }
            xl6OutputPuts("))", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_LIST ) {
            xl6OutputPuts("xvoListRemove(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("))", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_SET ) {
            xl6OutputPuts("xl6_xrt_set_remove_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_DICT ) {
            xl6OutputPuts("xvoTableRemove(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts(", 0)", ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_container_remove_value(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            if ( argCount == 2 && arg1 != NULL ) {
                xl6CgenExpr(ctx, arg1);
                xl6OutputPuts("), 1)", ctx->out);
            } else {
                xl6OutputPuts("1), 0)", ctx->out);
            }
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "removeValue") && argCount == 1 && arg0 != NULL ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xl6_xrt_typed_array_remove_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xl6_xrt_array_remove_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_VAR ) {
            xl6OutputPuts("xl6_xrt_value_remove_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenError(ctx, call, "removeValue requires array receiver");
            xl6OutputPuts("0", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "contains") && argCount == 1 && arg0 != NULL ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xl6_xrt_typed_array_contains_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_LIST ) {
            xl6OutputPuts("xl6_xrt_typed_list_contains_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_SET ) {
            xl6OutputPuts("xl6_xrt_typed_set_contains_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_DICT ) {
            xl6OutputPuts("xrtTypedDictExists(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts(", 0)", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_array_contains", receiver, arg0);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_LIST ) {
            xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_list_contains", receiver, arg0);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_SET ) {
            xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_set_contains", receiver, arg0);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_DICT ) {
            xl6OutputPuts("xvoTableExists(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts(", 0)", ctx->out);
        } else {
            xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_value_contains", receiver, arg0);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "add") && argCount == 1 && arg0 != NULL ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_SET ) {
            xl6OutputPuts("xl6_xrt_typed_set_add_value_release(", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_SET ) {
            xl6OutputPuts("xl6_xrt_set_add_value(", ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_container_add_value(", ctx->out);
        }
        xl6CgenContainerReceiverExpr(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenContainerValueArgExpr(ctx, arg0);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "pop") && argCount == 0 ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            if ( !xl6CgenTypedArrayValueToItemExpr(ctx, receiverType, "xl6_xrt_typed_array_pop_value_or_null", receiver, NULL) ) {
                xl6OutputPuts("xl6_xrt_typed_array_pop_value_or_null(", ctx->out);
                xl6CgenContainerReceiverExpr(ctx, receiver);
                xl6OutputPutc(')', ctx->out);
            }
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xl6_xrt_array_pop_value_or_null(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_container_pop_value(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "take") && argCount == 1 && arg0 != NULL ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            if ( !xl6CgenTypedArrayValueToItemExpr(ctx, receiverType, "xl6_xrt_typed_array_take_value_or_null", receiver, arg0) ) {
                xl6OutputPuts("xl6_xrt_typed_array_take_value_or_null(", ctx->out);
                xl6CgenContainerReceiverExpr(ctx, receiver);
                xl6OutputPuts(", (unsigned int)(", ctx->out);
                xl6CgenExpr(ctx, arg0);
                xl6OutputPuts(") + 1u)", ctx->out);
            }
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xl6_xrt_array_take_value_or_null(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("))", ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_LIST ) {
            xl6OutputPuts("xl6_xrt_typed_list_take_value_or_null(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("))", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_LIST ) {
            xl6OutputPuts("xl6_xrt_list_take_value_or_null(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("))", ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_DICT ) {
            xl6OutputPuts("xl6_xrt_typed_dict_take_value_or_null(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts(", 0)", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_DICT ) {
            xl6OutputPuts("xl6_xrt_table_take_value_or_null(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_container_take_value(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "clear") && argCount == 0 ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xrtTypedArrayClear(", ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_LIST ) {
            xl6OutputPuts("xrtTypedListClear(", ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_SET ) {
            xl6OutputPuts("xrtTypedSetClear(", ctx->out);
        } else if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_DICT ) {
            xl6OutputPuts("xrtTypedDictClear(", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xvoArrayClear(", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_LIST ) {
            xl6OutputPuts("xvoListClear(", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_SET ) {
            xl6OutputPuts("xvoSetClear(", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_DICT ) {
            xl6OutputPuts("xvoTableClear(", ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_container_clear(", ctx->out);
        }
        xl6CgenContainerReceiverExpr(ctx, receiver);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "indexOf") && argCount == 1 && arg0 != NULL ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xl6_xrt_typed_array_index_of_value_release(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenContainerValueArgExpr(ctx, arg0);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenError(ctx, call, "indexOf requires typed array receiver");
            xl6OutputPuts("-1", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "reverse") && argCount == 0 ) {
        if ( typed && kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xrtTypedArrayReverse(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenError(ctx, call, "reverse requires typed array receiver");
            xl6OutputPuts("0", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "reserve") && argCount == 1 && arg0 != NULL ) {
        if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xvoArrayAlloc(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("))", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_VAR ) {
            xl6OutputPuts("xl6_xrt_container_reserve(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("))", ctx->out);
        } else {
            xl6CgenError(ctx, call, "reserve requires dynamic array receiver");
            xl6OutputPuts("0", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(name, "swap") && argCount == 2 && arg0 != NULL && arg1 != NULL ) {
        if ( kind == XL6_CGEN_CONTAINER_METHOD_ARRAY ) {
            xl6OutputPuts("xvoArraySwap(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("), (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg1);
            xl6OutputPuts("))", ctx->out);
        } else if ( kind == XL6_CGEN_CONTAINER_METHOD_VAR ) {
            xl6OutputPuts("xl6_xrt_container_swap(", ctx->out);
            xl6CgenContainerReceiverExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg0);
            xl6OutputPuts("), (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, arg1);
            xl6OutputPuts("))", ctx->out);
        } else {
            xl6CgenError(ctx, call, "swap requires dynamic array receiver");
            xl6OutputPuts("0", ctx->out);
        }
        xl6AstFree(receiverType);
        return 1;
    }

    xl6CgenError(ctx, call, "invalid container method arguments");
    xl6OutputPuts("0", ctx->out);
    xl6AstFree(receiverType);
    return 1;
}

