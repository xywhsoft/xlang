static void xl6CgenEmitIndexReceiverExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* receiverName)
{
    if ( receiverName != NULL ) {
        xl6OutputPuts(receiverName, ctx->out);
    } else {
        xl6CgenExpr(ctx, receiver);
    }
}

static int xl6CgenIndexKeyIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* index)
{
    XL6_AstNode* typeNode;
    int result;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, index);
    result = xl6CgenIsXValueType(typeNode);
    xl6AstFree(typeNode);
    return result;
}

static int xl6CgenIndexKeyIsInteger(XL6_CgenContext* ctx, const XL6_AstNode* index)
{
    XL6_AstNode* typeNode;
    int result;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, index);
    result = typeNode != NULL && xl6CgenIsIntegerTypeName(typeNode->text);
    xl6AstFree(typeNode);
    return result;
}

static void xl6CgenValueIndexRaiseFailure(XL6_CgenContext* ctx, const XL6_AstNode* at, const char* message);

static void xl6CgenXValueDictGetExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* receiverName, const XL6_AstNode* index)
{
    if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
        xl6OutputPuts("xvoTableGetValue(", ctx->out);
        xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts(", 0)", ctx->out);
        return;
    }
    if ( !xl6CgenIndexKeyIsXValue(ctx, index) ) {
        xl6CgenError(ctx, index, "dict index key must be string or var string");
        xl6OutputPuts("xvoTableGetValue(0, \"\", 0)", ctx->out);
        return;
    }

    xl6OutputPuts("xl6_xrt_dict_get_var_key(", ctx->out);
    xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenXValueFromExpr(ctx, index);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenXValueSafeIndexGetExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* receiverName, const XL6_AstNode* index)
{
    if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
        xl6OutputPuts("xl6_record_field_get_owned(", ctx->out);
        xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts(", 0", ctx->out);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    if ( !xl6CgenIndexKeyIsXValue(ctx, index) ) {
        xl6OutputPuts("xl6_xrt_index_safe_get_i64(", ctx->out);
        xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts("))", ctx->out);
        return;
    }

    xl6OutputPuts("xl6_xrt_index_safe_get_var_key(", ctx->out);
    xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenXValueFromExpr(ctx, index);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenTypedDictGetRawExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index)
{
    if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
        xl6OutputPuts("xrtTypedDictGet(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts(", 0)", ctx->out);
        return;
    }
    if ( !xl6CgenIndexKeyIsXValue(ctx, index) ) {
        xl6CgenError(ctx, index, "dict index key must be string or var string");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    xl6OutputPuts("xl6_xrt_typed_dict_get_var_key(", ctx->out);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenXValueFromExpr(ctx, index);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenTypedDictSetDynamicKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName)
{
    int id;

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_dict_key_%d = ", id);
    xl6CgenXValueFromExpr(ctx, index);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!(__xl6_dict_key_%d != 0 && xvoType(__xl6_dict_key_%d) == XL6_XVO_DT_TEXT && xrtTypedDictSetValue(", id, id);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPrintf(ctx->out, ", xvoGetText(__xl6_dict_key_%d), 0, %s))) {\n", id, tempName != NULL ? tempName : "0");
    ctx->indent += 1;
    xl6CgenValueIndexRaiseFailure(ctx, index, "dict index assignment failed");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_dict_key_%d != 0) { xvoUnref(__xl6_dict_key_%d); }\n", id, id);
    if ( tempName != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenValueIndexRaiseFailure(XL6_CgenContext* ctx, const XL6_AstNode* at, const char* message)
{
    xl6CgenIndent(ctx);
    if ( ctx != NULL && ctx->needsErrorRuntime ) {
        xl6OutputPrintf(ctx->out,
                "xl6_error_raise(\"RangeError\", \"%s\", \"\", %d, %d);\n",
                message != NULL ? message : "container index operation failed",
                at != NULL ? at->line : 0,
                at != NULL ? at->column : 0);
    } else {
        xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
    }
}

static void xl6CgenXValueDictSetDynamicKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName)
{
    int id;

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_dict_key_%d = ", id);
    xl6CgenXValueFromExpr(ctx, index);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!(__xl6_dict_key_%d != 0 && xvoType(__xl6_dict_key_%d) == XL6_XVO_DT_TEXT && xvoTableSetValue(", id, id);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPrintf(ctx->out, ", xvoGetText(__xl6_dict_key_%d), 0, %s, true)) && %s != 0) {\n", id, tempName != NULL ? tempName : "0", tempName != NULL ? tempName : "0");
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
    xl6CgenValueIndexRaiseFailure(ctx, index, "dict index assignment failed");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_dict_key_%d != 0) { xvoUnref(__xl6_dict_key_%d); }\n", id, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenXValueIndexSetI64Stmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName)
{
    int id;

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_index_values_%d = ", id);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "int __xl6_index_type_%d = (__xl6_index_values_%d != 0 ? xvoType(__xl6_index_values_%d) : 0);\n", id, id, id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!((__xl6_index_type_%d == XL6_XVO_DT_ARRAY && xvoIndexSetI64(__xl6_index_values_%d, (long long)(", id, id);
    xl6CgenExpr(ctx, index);
    xl6OutputPrintf(ctx->out, "), %s, true)) || (__xl6_index_type_%d == XL6_XVO_DT_LIST && xvoListSetValue(__xl6_index_values_%d, (long long)(", tempName != NULL ? tempName : "0", id, id);
    xl6CgenExpr(ctx, index);
    xl6OutputPrintf(ctx->out, "), %s, true))) && %s != 0) {\n", tempName != NULL ? tempName : "0", tempName != NULL ? tempName : "0");
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
    xl6CgenValueIndexRaiseFailure(ctx, index, "array or list index assignment failed");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenXValueIndexSetDynamicKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName)
{
    int id;

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_index_values_%d = ", id);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_index_key_%d = ", id);
    xl6CgenXValueFromExpr(ctx, index);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "int __xl6_index_type_%d = (__xl6_index_values_%d != 0 ? xvoType(__xl6_index_values_%d) : 0);\n", id, id, id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "bool __xl6_index_ok_%d = false;\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "bool __xl6_index_value_moved_%d = false;\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_index_key_%d != 0 && xvoType(__xl6_index_key_%d) == XL6_XVO_DT_TEXT) {\n", id, id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_index_ok_%d = xl6_record_field_set_borrow(__xl6_index_values_%d, xvoGetText(__xl6_index_key_%d), %s);\n", id, id, id, tempName != NULL ? tempName : "0");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "long long __xl6_index_i_%d = (__xl6_index_key_%d != 0 ? (long long)xvoGetInt(__xl6_index_key_%d) : 0);\n", id, id, id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_index_ok_%d = (__xl6_index_type_%d == XL6_XVO_DT_ARRAY && xvoIndexSetI64(__xl6_index_values_%d, __xl6_index_i_%d, %s, true)) || (__xl6_index_type_%d == XL6_XVO_DT_LIST && xvoListSetValue(__xl6_index_values_%d, __xl6_index_i_%d, %s, true));\n", id, id, id, id, tempName != NULL ? tempName : "0", id, id, id, tempName != NULL ? tempName : "0");
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_index_ok_%d) { __xl6_index_value_moved_%d = true; }\n", id, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!__xl6_index_ok_%d && %s != 0) {\n", id, tempName != NULL ? tempName : "0");
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
    if ( tempName != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", tempName);
    }
    xl6CgenValueIndexRaiseFailure(ctx, index, "container index assignment failed");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_index_key_%d != 0) { xvoUnref(__xl6_index_key_%d); }\n", id, id);
    if ( tempName != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (!__xl6_index_value_moved_%d && %s != 0) { xvoUnref(%s); }\n", id, tempName, tempName);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static int xl6CgenDynamicIndexReceiverCanOwnResult(const XL6_AstNode* node, const XL6_AstNode* index, const XL6_AstNode* receiverType)
{
    if ( receiverType == NULL || receiverType->text == NULL ) {
        return 0;
    }
    if ( node != NULL && node->kind == XL6_AST_SAFE_INDEX ) {
        if ( xl6CgenIsDictXValueType(receiverType) || xl6CTextEquals(receiverType->text, "var") ) {
            return 1;
        }
        if ( index != NULL && !xl6CgenIsDictXValueType(receiverType) ) {
            return xl6CgenIsArrayXValueType(receiverType) || xl6CgenIsListXValueType(receiverType);
        }
        return 0;
    }
    return xl6CgenIsArrayXValueType(receiverType) || xl6CgenIsListXValueType(receiverType) || xl6CgenIsDictXValueType(receiverType) || xl6CTextEquals(receiverType->text, "var");
}

static void xl6CgenDynamicIndexBorrowExpr(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* receiver, const char* receiverName, const XL6_AstNode* index, const XL6_AstNode* receiverType)
{
    if ( node != NULL && node->kind == XL6_AST_SAFE_INDEX ) {
        xl6CgenXValueSafeIndexGetExpr(ctx, receiver, receiverName, index);
    } else if ( xl6CgenIsArrayXValueType(receiverType) ) {
        xl6OutputPuts("xvoIndexGetI64(", ctx->out);
        xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsListXValueType(receiverType) ) {
        xl6OutputPuts("xvoListGetValue(", ctx->out);
        xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsDictXValueType(receiverType) ) {
        xl6CgenXValueDictGetExpr(ctx, receiver, receiverName, index);
    } else if ( receiverType != NULL && xl6CTextEquals(receiverType->text, "var") ) {
        xl6CgenXValueSafeIndexGetExpr(ctx, receiver, receiverName, index);
    } else {
        xl6CgenError(ctx, node, "index receiver must be array, list, dict or var");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    }
}

static void xl6CgenDynamicIndexRetainReleaseExpr(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* receiver, const XL6_AstNode* index, const XL6_AstNode* receiverType)
{
    if ( node != NULL && node->kind == XL6_AST_SAFE_INDEX ) {
        if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6OutputPuts("xl6_record_field_get_owned(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts(", 1", ctx->out);
            xl6OutputPutc(')', ctx->out);
        } else if ( !xl6CgenIndexKeyIsXValue(ctx, index) ) {
            xl6OutputPuts("xl6_xrt_index_safe_get_i64_retain_release(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts("))", ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_index_safe_get_var_key_retain_release(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenXValueFromExpr(ctx, index);
            xl6OutputPutc(')', ctx->out);
        }
    } else if ( xl6CgenIsArrayXValueType(receiverType) ) {
        xl6OutputPuts("xl6_xrt_array_index_retain_release(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsListXValueType(receiverType) ) {
        xl6OutputPuts("xl6_xrt_list_index_retain_release(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsDictXValueType(receiverType) ) {
        if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6OutputPuts("xl6_xrt_dict_get_key_retain_release(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CgenIndexKeyIsXValue(ctx, index) ) {
            xl6OutputPuts("xl6_xrt_dict_get_var_key_retain_release(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenXValueFromExpr(ctx, index);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenError(ctx, index, "dict index key must be string or var string");
            xl6OutputPuts("xl6_xrt_null_release(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPutc(')', ctx->out);
        }
    } else if ( receiverType != NULL && xl6CTextEquals(receiverType->text, "var") ) {
        if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6OutputPuts("xl6_record_field_get_owned(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts(", 1", ctx->out);
            xl6OutputPutc(')', ctx->out);
        } else if ( !xl6CgenIndexKeyIsXValue(ctx, index) ) {
            xl6OutputPuts("xl6_xrt_index_safe_get_i64_retain_release(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts("))", ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_index_safe_get_var_key_retain_release(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenXValueFromExpr(ctx, index);
            xl6OutputPutc(')', ctx->out);
        }
    } else {
        xl6CgenError(ctx, node, "index receiver must be array, list, dict or var");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    }
}

static void xl6CgenIndexExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    XL6_AstNode* receiverType;

    receiver = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    index = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);

    if ( xl6CgenClassPropertyIndexExpr(ctx, node) ) {
        xl6AstFree(receiverType);
        return;
    }

    if ( xl6CgenClassProtocolIndexExpr(ctx, node) ) {
        xl6AstFree(receiverType);
        return;
    }

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) ) {
        const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, receiverType);
        const XL6_AstNode* itemType = xl6CgenTypedContainerItemType(ctx, receiverType);
        const char* typeName = concreteType != NULL ? concreteType->text : NULL;
        const char* itemCType = itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue";
        int itemIsFunction = xl6CgenTypeContainsFunctionType(itemType);
        int itemIsLanguageBool = xl6CgenIsLanguageBoolType(ctx, itemType);
        int itemIsString = itemType != NULL && xl6CTextEquals(itemType->text, "string");

        if ( node != NULL && node->kind == XL6_AST_SAFE_INDEX ) {
            xl6OutputPuts("xl6_xrt_typed_box_or_null(", ctx->out);
            xl6CgenEmitXrtTypeDescRef(ctx, itemType);
            xl6OutputPuts(", ", ctx->out);
            if ( xl6CTextEquals(typeName, "array") ) {
                xl6OutputPuts("xl6_xrt_typed_array_get_at(", ctx->out);
                xl6CgenExpr(ctx, receiver);
                xl6OutputPuts(", (long long)(", ctx->out);
                xl6CgenExpr(ctx, index);
                xl6OutputPuts("))", ctx->out);
            } else if ( xl6CTextEquals(typeName, "list") ) {
                xl6OutputPuts("xrtTypedListGet(", ctx->out);
                xl6CgenExpr(ctx, receiver);
                xl6OutputPuts(", (long long)(", ctx->out);
                xl6CgenExpr(ctx, index);
                xl6OutputPuts("))", ctx->out);
            } else if ( xl6CTextEquals(typeName, "dict") ) {
                xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
            } else {
                xl6CgenError(ctx, node, "typed container safe index requires array, list or dict");
                xl6OutputPuts("0", ctx->out);
            }
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(typeName, "array") ) {
            if ( itemIsFunction ) {
                xl6OutputPuts("xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(", ctx->out);
            } else if ( itemIsLanguageBool ) {
                xl6OutputPuts("((int)(*(bool*)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(", ctx->out);
            } else if ( itemIsString ) {
                xl6OutputPuts("xl6_xrt_str_dup((*(const char**)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(", ctx->out);
            } else {
                xl6OutputPuts("(*(", ctx->out);
                xl6OutputPuts(itemCType, ctx->out);
                xl6OutputPuts("*)xl6_xrt_typed_require_item(xl6_xrt_typed_array_get_at(", ctx->out);
            }
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts((itemIsLanguageBool || itemIsString) ? ")))))" : "))))", ctx->out);
        } else if ( xl6CTextEquals(typeName, "list") ) {
            if ( itemIsFunction ) {
                xl6OutputPuts("xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(xrtTypedListGet(", ctx->out);
            } else if ( itemIsLanguageBool ) {
                xl6OutputPuts("((int)(*(bool*)xl6_xrt_typed_require_item(xrtTypedListGet(", ctx->out);
            } else if ( itemIsString ) {
                xl6OutputPuts("xl6_xrt_str_dup((*(const char**)xl6_xrt_typed_require_item(xrtTypedListGet(", ctx->out);
            } else {
                xl6OutputPuts("(*(", ctx->out);
                xl6OutputPuts(itemCType, ctx->out);
                xl6OutputPuts("*)xl6_xrt_typed_require_item(xrtTypedListGet(", ctx->out);
            }
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts((itemIsLanguageBool || itemIsString) ? ")))))" : "))))", ctx->out);
        } else if ( xl6CTextEquals(typeName, "dict") ) {
            if ( itemIsFunction ) {
                xl6OutputPuts("xl6_function_from_callable(*(xrt_callable**)xl6_xrt_typed_require_item(", ctx->out);
                xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
                xl6OutputPuts("))", ctx->out);
            } else if ( itemIsLanguageBool ) {
                xl6OutputPuts("((int)(*(bool*)xl6_xrt_typed_require_item(", ctx->out);
                xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
                xl6OutputPuts(")))", ctx->out);
            } else if ( itemIsString ) {
                xl6OutputPuts("xl6_xrt_str_dup((*(const char**)xl6_xrt_typed_require_item(", ctx->out);
                xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
                xl6OutputPuts(")))", ctx->out);
            } else {
                xl6OutputPuts("(*(", ctx->out);
                xl6OutputPuts(itemCType, ctx->out);
                xl6OutputPuts("*)xl6_xrt_typed_require_item(", ctx->out);
                xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
                xl6OutputPuts("))", ctx->out);
            }
        } else {
            xl6CgenError(ctx, node, "typed container index access requires array, list or dict");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
    } else if ( !xl6CgenExprIsXValueBorrow(ctx, receiver) && xl6CgenDynamicIndexReceiverCanOwnResult(node, index, receiverType) ) {
        xl6CgenDynamicIndexRetainReleaseExpr(ctx, node, receiver, index, receiverType);
    } else {
        xl6CgenDynamicIndexBorrowExpr(ctx, node, receiver, NULL, index, receiverType);
    }
    xl6AstFree(receiverType);
}
