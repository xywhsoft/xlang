static const char* xl6CgenContainsReleaseFuncName(const char* funcName)
{
    if ( funcName == NULL || xl6CTextEquals(funcName, "xl6_xrt_value_contains") ) {
        return "xl6_xrt_value_contains_release";
    }
    if ( xl6CTextEquals(funcName, "xl6_xrt_array_contains") ) {
        return "xl6_xrt_array_contains_release";
    }
    if ( xl6CTextEquals(funcName, "xl6_xrt_list_contains") ) {
        return "xl6_xrt_list_contains_release";
    }
    if ( xl6CTextEquals(funcName, "xl6_xrt_set_contains") ) {
        return "xl6_xrt_set_contains_release";
    }
    return "xl6_xrt_value_contains_release";
}

static void xl6CgenContainsXValueCallExpr(XL6_CgenContext* ctx, const char* funcName, const XL6_AstNode* values, const XL6_AstNode* item)
{
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, values) ) {
        xl6OutputPrintf(ctx->out, "%s(", xl6CgenContainsReleaseFuncName(funcName));
        xl6CgenXValueFromExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, item);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    if ( xl6CgenExprIsXValueBorrow(ctx, values) ) {
        xl6OutputPrintf(ctx->out, "%s(", funcName != NULL ? funcName : "xl6_xrt_value_contains");
        xl6CgenExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, item);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    xl6OutputPrintf(ctx->out, "%s(", xl6CgenContainsReleaseFuncName(funcName));
    xl6CgenExpr(ctx, values);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenXValueFromExpr(ctx, item);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenContainsXValueCallStmt(XL6_CgenContext* ctx, const char* funcName, const XL6_AstNode* values, const XL6_AstNode* item, const char* targetName)
{
    int id;

    if ( ctx == NULL || values == NULL || targetName == NULL ) {
        return 0;
    }

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, values) ) {
        id = ctx->tempId++;
        xl6CgenIndent(ctx);
        xl6OutputPuts("{\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue __xl6_in_values_%d = ", id);
        xl6CgenXValueFromExpr(ctx, values);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = %s(__xl6_in_values_%d, ", targetName, funcName != NULL ? funcName : "xl6_xrt_value_contains", id);
        xl6CgenXValueFromExpr(ctx, item);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_in_values_%d != 0) {\n", id);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_in_values_%d);\n", id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return 1;
    }

    if ( xl6CgenExprIsXValueBorrow(ctx, values) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = %s(", targetName, funcName != NULL ? funcName : "xl6_xrt_value_contains");
        xl6CgenExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, item);
        xl6OutputPuts(");\n", ctx->out);
        return 1;
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_in_values_%d = ", id);
    xl6CgenExpr(ctx, values);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = %s(__xl6_in_values_%d, ", targetName, funcName != NULL ? funcName : "xl6_xrt_value_contains", id);
    xl6CgenXValueFromExpr(ctx, item);
    xl6OutputPuts(");\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_in_values_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_in_values_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static void xl6CgenDictContainsKeyExpr(XL6_CgenContext* ctx, const XL6_AstNode* values, const XL6_AstNode* key)
{
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, values) ) {
        xl6OutputPuts("xl6_xrt_table_exists_release(", ctx->out);
        xl6CgenXValueFromExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, key);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    if ( xl6CgenExprIsXValueBorrow(ctx, values) ) {
        xl6OutputPuts("xvoTableExists(", ctx->out);
        xl6CgenExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, key);
        xl6OutputPuts(", 0)", ctx->out);
        return;
    }

    xl6OutputPuts("xl6_xrt_table_exists_release(", ctx->out);
    xl6CgenExpr(ctx, values);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenExpr(ctx, key);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenDictContainsKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* values, const XL6_AstNode* key, const char* targetName)
{
    int id;

    if ( ctx == NULL || values == NULL || key == NULL || targetName == NULL ) {
        return 0;
    }

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, values) ) {
        id = ctx->tempId++;
        xl6CgenIndent(ctx);
        xl6OutputPuts("{\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue __xl6_in_values_%d = ", id);
        xl6CgenXValueFromExpr(ctx, values);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xvoTableExists(__xl6_in_values_%d, ", targetName, id);
        xl6CgenExpr(ctx, key);
        xl6OutputPuts(", 0);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_in_values_%d != 0) {\n", id);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_in_values_%d);\n", id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return 1;
    }

    if ( xl6CgenExprIsXValueBorrow(ctx, values) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xvoTableExists(", targetName);
        xl6CgenExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, key);
        xl6OutputPuts(", 0);\n", ctx->out);
        return 1;
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_in_values_%d = ", id);
    xl6CgenExpr(ctx, values);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xvoTableExists(__xl6_in_values_%d, ", targetName, id);
    xl6CgenExpr(ctx, key);
    xl6OutputPuts(", 0);\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_in_values_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_in_values_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenTypedContainerContainsExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* values,
    const XL6_AstNode* item)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    if ( ctx == NULL || typeNode == NULL || values == NULL || item == NULL ||
         !xl6CgenExprUsesTypedContainerStorage(ctx, values) ) {
        return 0;
    }

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( typeName == NULL ) {
        return 0;
    }

    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_contains_value_release(", ctx->out);
        xl6CgenExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, item);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }

    if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_contains_value_release(", ctx->out);
        xl6CgenExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, item);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }

    if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_contains_value_release(", ctx->out);
        xl6CgenExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, item);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }

    if ( xl6CTextEquals(typeName, "dict") ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, item, "string") ) {
            xl6CgenError(ctx, item, "dict membership requires a string key");
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6OutputPuts("xrtTypedDictExists(", ctx->out);
            xl6CgenExpr(ctx, values);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, item);
            xl6OutputPuts(", 0)", ctx->out);
        }
        return 1;
    }

    return 0;
}

static int xl6CgenTypedContainerContainsStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* values,
    const XL6_AstNode* item,
    const char* targetName)
{
    if ( ctx == NULL || targetName == NULL ||
         !xl6CgenTypedContainerContainsExpr(ctx, typeNode, values, item) ) {
        return 0;
    }

    xl6OutputPutc(';', ctx->out);
    return 1;
}

static int xl6CgenTypedContainerTempContainsExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* values,
    const XL6_AstNode* item)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "array") ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_array_contains", values, item);
        return 1;
    }
    if ( xl6CTextEquals(typeName, "list") ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_list_contains", values, item);
        return 1;
    }
    if ( xl6CTextEquals(typeName, "set") ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_set_contains", values, item);
        return 1;
    }
    if ( xl6CTextEquals(typeName, "dict") ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, item, "string") ) {
            xl6CgenError(ctx, item, "dict membership requires a string key");
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6CgenDictContainsKeyExpr(ctx, values, item);
        }
        return 1;
    }

    return 0;
}

static int xl6CgenTypedContainerTempContainsStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* values,
    const XL6_AstNode* item,
    const char* targetName)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( xl6CTextEquals(typeName, "array") ) {
        return xl6CgenContainsXValueCallStmt(ctx, "xl6_xrt_array_contains", values, item, targetName);
    }
    if ( xl6CTextEquals(typeName, "list") ) {
        return xl6CgenContainsXValueCallStmt(ctx, "xl6_xrt_list_contains", values, item, targetName);
    }
    if ( xl6CTextEquals(typeName, "set") ) {
        return xl6CgenContainsXValueCallStmt(ctx, "xl6_xrt_set_contains", values, item, targetName);
    }
    if ( xl6CTextEquals(typeName, "dict") ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, item, "string") ) {
            xl6CgenError(ctx, item, "dict membership requires a string key");
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = 0;\n", targetName);
            return 1;
        }
        return xl6CgenDictContainsKeyStmt(ctx, values, item, targetName);
    }

    return 0;
}

static int xl6CgenTryInExprStmt(XL6_CgenContext* ctx, const XL6_AstNode* node, const char* targetName)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* rightType;

    if ( ctx == NULL || node == NULL || targetName == NULL ||
         node->kind != XL6_AST_BINARY || !xl6CTextEquals(node->text, "in") ) {
        return 0;
    }

    left = node->childCount > 0 ? node->children[0] : NULL;
    right = node->childCount > 1 ? node->children[1] : NULL;

    {
        XL6_AstNode* protocolReceiverType = right != NULL && right->kind == XL6_AST_NAME ?
                                            xl6CgenCloneExprTypeForInference(ctx, right) :
                                            NULL;
        XL6_AstNode* protocolCall = xl6CgenProtocolTempCallNew(node);
        XL6_AstNode* protocolReturnType;
        if ( protocolCall != NULL && left != NULL ) {
            xl6CgenProtocolTempCallAddArg(protocolCall, left);
        }
        protocolReturnType = xl6CgenCloneClassProtocolReturnTypeForCall(ctx, protocolReceiverType, "__contains__", protocolCall);
        int hasProtocol = protocolReturnType != NULL;
        xl6CgenProtocolTempCallFree(protocolCall);
        xl6AstFree(protocolReceiverType);
        xl6AstFree(protocolReturnType);
        if ( hasProtocol ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = ", targetName);
            xl6CgenClassProtocolBinaryExpr(ctx, node);
            xl6OutputPuts(";\n", ctx->out);
            return 1;
        }
    }

    if ( right != NULL && xl6CgenIsRangeCall(right) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", targetName);
        xl6CgenRangeContainsExpr(ctx, left, right);
        xl6OutputPuts(";\n", ctx->out);
        return 1;
    }

    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    if ( rightType == NULL || rightType->text == NULL ) {
        xl6CgenError(ctx, node, "当前 C 生成 in 右侧类型必须可静态确定，或为 var");
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", targetName);
        xl6AstFree(rightType);
        return 1;
    }

    if ( xl6CTextEquals(rightType->text, "string") ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, left, "string") ) {
            xl6CgenError(ctx, node, "字符串 in 判断左侧必须是 string");
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = 0;\n", targetName);
        } else {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = (xrtFindStr(", targetName);
            xl6CgenExpr(ctx, right);
            xl6OutputPuts(", 0, ", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", 0, false) != 0);\n", ctx->out);
        }
        xl6AstFree(rightType);
        return 1;
    }

    if ( xl6CgenIsTypedContainerType(ctx, rightType) &&
         xl6CgenExprUsesTypedContainerStorage(ctx, right) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", targetName);
        if ( xl6CgenTypedContainerContainsStmt(ctx, rightType, right, left, targetName) ) {
            xl6OutputPutc('\n', ctx->out);
        } else {
            xl6OutputPuts("0;\n", ctx->out);
        }
        xl6AstFree(rightType);
        return 1;
    }

    if ( xl6CgenIsTypedContainerType(ctx, rightType) ) {
        if ( xl6CgenTypedContainerTempContainsStmt(ctx, rightType, right, left, targetName) ) {
            xl6AstFree(rightType);
            return 1;
        }
    }

    if ( xl6CgenIsArrayXValueType(rightType) ) {
        xl6AstFree(rightType);
        return xl6CgenContainsXValueCallStmt(ctx, "xl6_xrt_array_contains", right, left, targetName);
    }

    if ( xl6CTextEquals(rightType->text, "list") || xl6CTextEquals(rightType->text, "list<var>") ) {
        xl6AstFree(rightType);
        return xl6CgenContainsXValueCallStmt(ctx, "xl6_xrt_list_contains", right, left, targetName);
    }

    if ( xl6CTextEquals(rightType->text, "set") || xl6CTextEquals(rightType->text, "set<var>") ) {
        xl6AstFree(rightType);
        return xl6CgenContainsXValueCallStmt(ctx, "xl6_xrt_set_contains", right, left, targetName);
    }

    if ( xl6CgenIsDictXValueType(rightType) ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, left, "string") ) {
            xl6CgenError(ctx, node, "dict membership requires a string key");
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = 0;\n", targetName);
            xl6AstFree(rightType);
            return 1;
        }
        xl6AstFree(rightType);
        return xl6CgenDictContainsKeyStmt(ctx, right, left, targetName);
    }

    if ( xl6CTextEquals(rightType->text, "var") ) {
        xl6AstFree(rightType);
        return xl6CgenContainsXValueCallStmt(ctx, "xl6_xrt_value_contains", right, left, targetName);
    }

    xl6CgenError(ctx, node, "当前 C 生成 in 右侧只支持 string、array、list、set、dict、range 或 var");
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = 0;\n", targetName);
    xl6AstFree(rightType);
    return 1;
}
static void xl6CgenInExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* rightType;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( right != NULL && xl6CgenIsRangeCall(right) ) {
        xl6CgenRangeContainsExpr(ctx, left, right);
        return;
    }

    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    if ( rightType == NULL || rightType->text == NULL ) {
        xl6CgenError(ctx, node, "当前 C 生成 in 右侧类型必须可静态确定，或为 var");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "string") ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, left, "string") ) {
            xl6CgenError(ctx, node, "字符串 in 判断左侧必须是 string");
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6OutputPuts("(xrtFindStr(", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPuts(", 0, ", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", 0, false) != 0)", ctx->out);
        }
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CgenIsTypedContainerType(ctx, rightType) &&
         xl6CgenExprUsesTypedContainerStorage(ctx, right) ) {
        if ( !xl6CgenTypedContainerContainsExpr(ctx, rightType, right, left) ) {
            xl6OutputPuts("0", ctx->out);
        }
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CgenIsTypedContainerType(ctx, rightType) ) {
        if ( xl6CgenTypedContainerTempContainsExpr(ctx, rightType, right, left) ) {
            xl6AstFree(rightType);
            return;
        }
    }

    if ( xl6CgenIsArrayXValueType(rightType) ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_array_contains", right, left);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "list") || xl6CTextEquals(rightType->text, "list<var>") ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_list_contains", right, left);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "set") || xl6CTextEquals(rightType->text, "set<var>") ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_set_contains", right, left);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CgenIsDictXValueType(rightType) ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, left, "string") ) {
            xl6CgenError(ctx, node, "dict membership requires a string key");
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6CgenDictContainsKeyExpr(ctx, right, left);
        }
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "var") ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_value_contains", right, left);
        xl6AstFree(rightType);
        return;
    }

    xl6CgenError(ctx, node, "当前 C 生成 in 右侧只支持 string、array、list、set、dict、range 或 var");
    xl6OutputPuts("0", ctx->out);
    xl6AstFree(rightType);
}

static void xl6CgenArrayLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    xl6OutputPrintf(ctx->out, "xl6_xrt_array_varargs(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, node->children[i]);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenListLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    xl6OutputPrintf(ctx->out, "xl6_xrt_list_literal(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        const XL6_AstNode* pair = node->children[i];
        const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
        const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;

        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, key);
        xl6OutputPuts("), ", ctx->out);
        xl6CgenXValueFromExpr(ctx, value);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenSetLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    xl6OutputPrintf(ctx->out, "xl6_xrt_set_literal(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, node->children[i]);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenDictLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    xl6OutputPrintf(ctx->out, "xl6_xrt_dict_varargs(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        const XL6_AstNode* pair = node->children[i];
        const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
        const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;

        xl6OutputPuts(", ", ctx->out);
        if ( !xl6CgenExprStaticTypeIs(ctx, key, "string") ) {
            xl6CgenError(ctx, key, "当前 C 生成 dict 字面量键只支持 string");
            xl6OutputPuts("\"\"", ctx->out);
        } else {
            xl6CgenExpr(ctx, key);
        }
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, value);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenTypedLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* literal = node != NULL && node->childCount > 0 ? node->children[0] : NULL;

    if ( xl6CTextEquals(node != NULL ? node->text : NULL, "array") ) {
        xl6CgenArrayLiteralExpr(ctx, literal);
    } else if ( xl6CTextEquals(node != NULL ? node->text : NULL, "list") ) {
        xl6CgenListLiteralExpr(ctx, literal);
    } else if ( xl6CTextEquals(node != NULL ? node->text : NULL, "set") ) {
        xl6CgenSetLiteralExpr(ctx, literal);
    } else if ( xl6CTextEquals(node != NULL ? node->text : NULL, "dict") ) {
        xl6CgenDictLiteralExpr(ctx, literal);
    } else {
        xl6CgenError(ctx, node, "unknown typed container literal prefix");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    }
}

static int xl6CgenArrayVarargCount(const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int positionalIndex;
    int count;

    positionalIndex = 0;
    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG ) {
            continue;
        }
        if ( positionalIndex >= fixedCount ) {
            count += 1;
        }
        positionalIndex += 1;
    }
    return count;
}

static void xl6CgenArrayVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int positionalIndex;
    int count;
    int emitted;

    count = xl6CgenArrayVarargCount(args, fixedCount);
    xl6OutputPrintf(ctx->out, "xl6_xrt_array_varargs(%d", count);
    positionalIndex = 0;
    emitted = 0;
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG ) {
                continue;
            }
            if ( positionalIndex >= fixedCount ) {
                xl6OutputPuts(", ", ctx->out);
                xl6CgenXValueFromExpr(ctx, xl6CgenArgExpr(arg));
                emitted += 1;
            }
            positionalIndex += 1;
        }
    }
    (void)emitted;
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenDictVarargCount(const XL6_AstNode* params, const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int count;

    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG &&
             xl6CgenFindParamIndex(params, fixedCount, arg->text) < 0 ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenDictVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int count;

    count = xl6CgenDictVarargCount(params, args, fixedCount);
    xl6OutputPrintf(ctx->out, "xl6_xrt_dict_varargs(%d", count);
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL || arg->kind != XL6_AST_NAMED_ARG ||
                 xl6CgenFindParamIndex(params, fixedCount, arg->text) >= 0 ) {
                continue;
            }
            xl6OutputPuts(", ", ctx->out);
            xl6CgenCStringLiteral(ctx->out, arg->text);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenXValueFromExpr(ctx, xl6CgenArgExpr(arg));
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenFixedParamXValueConvertArgExpr(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr)
{
    XL6_AstNode* exprType;
    const XL6_AstNode* sourceType;
    const XL6_AstNode* concreteTarget;
    const char* target;

    if ( ctx == NULL || targetType == NULL || expr == NULL ) {
        return 0;
    }

    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    sourceType = xl6CgenResolveAliasType(ctx, exprType);
    concreteTarget = xl6CgenResolveAliasType(ctx, targetType);
    target = concreteTarget != NULL ? concreteTarget->text : NULL;

    if ( sourceType == NULL ||
         (!xl6CgenIsXValueType(sourceType) && !xl6CgenBinaryArithmeticIsXValue(ctx, expr)) ||
         target == NULL ) {
        xl6AstFree(exprType);
        return 0;
    }

    if ( xl6CgenIsIntegerTypeName(target) ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetInt", expr);
    } else if ( xl6CgenIsFloatTypeName(target) ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetFloat", expr);
    } else if ( xl6CgenIsBoolTypeName(target) ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetBool", expr);
    } else if ( xl6CTextEquals(target, "string") ) {
        if ( xl6CgenExprIsXValueBorrow(ctx, expr) ) {
            xl6OutputPuts("xl6_xrt_value_to_string(", ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_value_to_string_release(", ctx->out);
        }
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(target, "time") ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetTime", expr);
    } else if ( xl6CTextEquals(target, "point") ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetPoint", expr);
    } else {
        xl6AstFree(exprType);
        return 0;
    }

    xl6AstFree(exprType);
    return 1;
}

/*
    string 的所有入口都先把动态值解箱为文本，再由调用方复制或接管。
    不能把 xvalue 句柄直接当作 const char* 交给 XRT。
*/
static void xl6CgenStringSourceExpr(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr)
{
    if ( !xl6CgenFixedParamXValueConvertArgExpr(ctx, targetType, expr) ) {
        xl6CgenExpr(ctx, expr);
    }
}

/*
    普通函数与函数值调用必须共用同一套参数所有权规则。
    尤其是 var 实参，需要为被调函数移交一个 owned xvalue 引用。
*/
static void xl6CgenFixedTypeArgExpr(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr)
{
    XL6_AstNode* canonicalType;

    canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, targetType);

    if ( canonicalType != NULL &&
         xl6CgenIsTypedContainerType(ctx, canonicalType) &&
         xl6CgenExprUsesTypedContainerStorage(ctx, expr) ) {
        xl6CgenExpr(ctx, expr);
        xl6AstFree(canonicalType);
        return;
    }
    if ( canonicalType != NULL && xl6CgenIsFunctionType(canonicalType) ) {
        /*
            命名函数、匿名函数和已有函数值必须统一生成 XL6_Function。
            直接走普通表达式会把命名函数原文写进 C，既没有符号改写，也没有签名适配。
        */
        xl6CgenFunctionValueExpr(ctx, expr, canonicalType);
        xl6AstFree(canonicalType);
        return;
    }
    if ( canonicalType != NULL && xl6CgenIsXValueType(canonicalType) ) {
        xl6CgenXValueFromExprForTarget(ctx, canonicalType, expr);
        xl6AstFree(canonicalType);
        return;
    }
    if ( canonicalType != NULL && xl6CgenClassUpcastExpr(ctx, canonicalType, expr) ) {
        xl6AstFree(canonicalType);
        return;
    }
    if ( xl6CgenFixedParamXValueConvertArgExpr(ctx, canonicalType, expr) ) {
        xl6AstFree(canonicalType);
        return;
    }
    xl6AstFree(canonicalType);
    xl6CgenExpr(ctx, expr);
}

static void xl6CgenFixedParamArgExpr(XL6_CgenContext* ctx, const XL6_AstNode* param, const XL6_AstNode* expr)
{
    const XL6_AstNode* targetType;

    targetType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
    xl6CgenFixedTypeArgExpr(ctx, targetType, expr);
}

static void xl6CgenKnownCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* args)
{
    const XL6_AstNode* params;
    int fixedCount;
    int hasVararg;
    int i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    hasVararg = params != NULL && params->childCount > (size_t)fixedCount;

    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* expr = xl6CgenFindArgForParam(args, param->text, i);

        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( expr != NULL ) {
            xl6CgenFixedParamArgExpr(ctx, param, expr);
        } else if ( param->childCount > 1 ) {
            xl6CgenFixedParamArgExpr(ctx, param, param->children[1]);
        } else {
            xl6CgenError(ctx, param, "missing required argument for parameter");
            xl6OutputPuts("0", ctx->out);
        }
    }
    if ( hasVararg ) {
        const XL6_AstNode* vararg = params->children[fixedCount];
        if ( fixedCount > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( xl6CgenIsArrayXValueType(vararg->children[0]) ) {
            xl6CgenArrayVarargExpr(ctx, args, fixedCount);
        } else if ( xl6CgenIsDictXValueType(vararg->children[0]) ) {
            xl6CgenDictVarargExpr(ctx, params, args, fixedCount);
        } else {
            xl6CgenError(ctx, vararg, "未知可变参数类型");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
    }
}

static void xl6CgenParamNodeArgs(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* args)
{
    int fixedCount;
    int i;

    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount ) {
        xl6CgenError(ctx, params, "当前 C 生成暂不支持可变参数调用");
    }
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* expr = xl6CgenFindArgForParam(args, param->text, i);
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( expr != NULL ) {
            xl6CgenFixedParamArgExpr(ctx, param, expr);
        } else if ( param->childCount > 1 ) {
            xl6CgenFixedParamArgExpr(ctx, param, param->children[1]);
        } else {
            xl6CgenError(ctx, param, "missing required argument for parameter");
            xl6OutputPuts("0", ctx->out);
        }
    }
}
