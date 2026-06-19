static void xl6CgenIndexAssignTemp(XL6_CgenContext* ctx, const XL6_AstNode* left, const char* tempName)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    XL6_AstNode* receiverType;

    receiver = left != NULL && left->childCount > 0 ? left->children[0] : NULL;
    index = left != NULL && left->childCount > 1 ? left->children[1] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);

    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( !", ctx->out);
    if ( xl6CgenIsArrayXValueType(receiverType) ) {
        xl6OutputPuts("xvoArraySetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", (unsigned int)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPrintf(ctx->out, "), %s, true)", tempName != NULL ? tempName : "0");
    } else if ( xl6CgenIsListXValueType(receiverType) ) {
        xl6OutputPuts("xvoListSetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPrintf(ctx->out, "), %s, true)", tempName != NULL ? tempName : "0");
    } else if ( xl6CgenIsDictXValueType(receiverType) ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6CgenError(ctx, index, "dict 索引键必须是 string");
            xl6OutputPrintf(ctx->out, "false");
        } else {
            xl6OutputPuts("xvoTableSetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPrintf(ctx->out, ", 0, %s, true)", tempName != NULL ? tempName : "0");
        }
    } else {
        xl6CgenError(ctx, left, "code generation error");
        xl6OutputPuts("false", ctx->out);
    }
    xl6OutputPrintf(ctx->out, " && %s != 0 ) {\n", tempName != NULL ? tempName : "0");
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    xl6AstFree(receiverType);
}

static void xl6CgenIndexAssignStmt(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    int id;
    char tempName[64];

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_index_assign_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = ", tempName);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndexAssignTemp(ctx, left, tempName);
}

static const char* xl6CgenCompoundBinaryOp(const char* op)
{
    if ( xl6CTextEquals(op, "+=") ) {
        return "+";
    }
    if ( xl6CTextEquals(op, "-=") ) {
        return "-";
    }
    if ( xl6CTextEquals(op, "*=") ) {
        return "*";
    }
    if ( xl6CTextEquals(op, "/=") ) {
        return "/";
    }
    if ( xl6CTextEquals(op, "%=") ) {
        return "%";
    }
    return NULL;
}

static int xl6CgenExprTypeIsFloat(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    int result;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    result = typeNode != NULL && xl6CgenIsFloatTypeName(typeNode->text);
    xl6AstFree(typeNode);
    return result;
}

static void xl6CgenXValueAsI64Expr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPuts("xvoGetInt(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsFloatTypeName(typeNode->text) ) {
        xl6OutputPuts("(long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenExpr(ctx, expr);
    }
    xl6AstFree(typeNode);
}

static void xl6CgenXValueAsF64Expr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPuts("xl6_xrt_value_to_double(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(", xvoType(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else {
        xl6OutputPuts("(double)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    }
    xl6AstFree(typeNode);
}

static void xl6CgenIndexCompoundAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right)
{
    int id;
    int rightIsFloat;
    const char* binaryOp;
    char currentName[64];
    char rightName[64];
    char typeName[64];
    char tempName[64];

    if ( xl6CTextEquals(op, "=") ) {
        xl6CgenIndexAssignStmt(ctx, left, right);
        return;
    }

    id = ctx->tempId++;
    snprintf(currentName, sizeof(currentName), "__xl6_index_current_%d", id);
    snprintf(rightName, sizeof(rightName), "__xl6_index_right_%d", id);
    snprintf(typeName, sizeof(typeName), "__xl6_index_type_%d", id);
    snprintf(tempName, sizeof(tempName), "__xl6_index_assign_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = ", currentName);
    xl6CgenIndexExpr(ctx, left);
    xl6OutputPuts(";\n", ctx->out);

    if ( xl6CTextEquals(op, "?=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (xvoIsNull(%s)) {\n", currentName);
        ctx->indent += 1;
        xl6CgenIndexAssignStmt(ctx, left, right);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }

    if ( xl6CTextEquals(op, "**=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = (long long)(", rightName);
        xl6CgenXValueAsI64Expr(ctx, right);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateInt(xl6_int_pow_i64(xvoGetInt(%s), %s));\n",
                tempName, currentName, rightName);
        xl6CgenIndexAssignTemp(ctx, left, tempName);
        return;
    }

    binaryOp = xl6CgenCompoundBinaryOp(op);
    if ( binaryOp == NULL ) {
        xl6CgenError(ctx, left, "当前 C 生成不支持该索引复合赋值运算符");
        return;
    }

    rightIsFloat = xl6CgenExprTypeIsFloat(ctx, right);
    if ( rightIsFloat ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "double %s = ", rightName);
        xl6CgenXValueAsF64Expr(ctx, right);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "int %s = xvoType(%s);\n", typeName, currentName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                "xvalue %s = xvoCreateFloat(xl6_xrt_value_to_double(%s, %s) %s %s);\n",
                tempName, currentName, typeName, binaryOp, rightName);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = (long long)(", rightName);
        xl6CgenXValueAsI64Expr(ctx, right);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "int %s = xvoType(%s);\n", typeName, currentName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", tempName);
        if ( xl6CTextEquals(op, "%=") ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = xvoCreateInt(xvoGetInt(%s) %% %s);\n",
                    tempName, currentName, rightName);
        } else {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s == XL6_XVO_DT_FLOAT) {\n", typeName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out,
                    "%s = xvoCreateFloat(xl6_xrt_value_to_double(%s, %s) %s (double)%s);\n",
                    tempName, currentName, typeName, binaryOp, rightName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("} else {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = xvoCreateInt(xvoGetInt(%s) %s %s);\n",
                    tempName, currentName, binaryOp, rightName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
    }

    xl6CgenIndexAssignTemp(ctx, left, tempName);
}

static void xl6CgenAssignStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    const char* op;

    left = node->childCount > 0 ? node->children[0] : NULL;
    right = node->childCount > 1 ? node->children[1] : NULL;
    op = node->text != NULL ? node->text : "=";

    if ( left == NULL || (left->kind != XL6_AST_NAME && left->kind != XL6_AST_MEMBER && left->kind != XL6_AST_INDEX) ) {
        xl6CgenError(ctx, node, "code generation error");
        return;
    }
    if ( left->kind == XL6_AST_INDEX ) {
        xl6CgenIndexCompoundAssignStmt(ctx, left, op, right);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* ctorCustom;
        const XL6_AstNode* fieldType;
        XL6_AstNode* fieldCustom;

        ctorCustom = NULL;
        if ( xl6CgenCallIsCustomCtor(ctx, right, &ctorCustom) ) {
            fieldType = xl6CgenMemberFieldType(ctx, left);
            fieldCustom = fieldType != NULL ? xl6CgenFindCustom(ctx, fieldType->text) : NULL;
            if ( fieldCustom == NULL ) {
                xl6CgenError(ctx, left, "custom 构造只能赋给同类型 custom 字段");
                return;
            }
            if ( !xl6CTextEquals(fieldCustom->text, ctorCustom->text) ) {
                xl6CgenError(ctx, right, "code generation error");
                return;
            }
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".handle = ", ctx->out);
            xl6CgenCustomCtorValue(ctx, ctorCustom, right);
            xl6OutputPuts(";\n", ctx->out);
            return;
        }
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsFunctionType(xl6CgenFindVarType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];
        char tempName[64];
        const XL6_AstNode* targetType = xl6CgenFindVarType(ctx, left->text);

        xl6CgenCIdentText(cName, sizeof(cName), left->text);
        snprintf(tempName, sizeof(tempName), "__xl6_function_assign_%d", id);

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "XL6_Function %s = ", tempName);
        xl6CgenFunctionValueExpr(ctx, right, targetType);
        xl6OutputPuts(";\n", ctx->out);
        if ( xl6CgenExprIsFunctionBorrow(ctx, right) ) {
            xl6CgenRetainFunctionValue(ctx, tempName);
        }
        xl6CgenReleaseFunctionValue(ctx, left->text);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsXValueType(xl6CgenFindVarType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];
        const XL6_AstNode* targetType = xl6CgenFindVarType(ctx, left->text);

        xl6CgenCIdentText(cName, sizeof(cName), left->text);

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue __xl6_assign_%d = ", id);
        xl6CgenXValueFromExprForTarget(ctx, targetType, right);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", cName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = __xl6_assign_%d;\n", cName, id);
        return;
    }

    xl6CgenIndent(ctx);
    if ( xl6CTextEquals(op, "?=") ) {
        if ( left->kind == XL6_AST_NAME && xl6CgenIsXValueType(xl6CgenFindVarType(ctx, left->text)) ) {
            int id = ctx->tempId++;
            char cName[512];
            const XL6_AstNode* targetType = xl6CgenFindVarType(ctx, left->text);

            xl6CgenCIdentText(cName, sizeof(cName), left->text);
            xl6OutputPrintf(ctx->out, "if (xvoIsNull(%s)) {\n", cName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvalue __xl6_assign_%d = ", id);
            xl6CgenXValueFromExprForTarget(ctx, targetType, right);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", cName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", cName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = __xl6_assign_%d;\n", cName, id);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            return;
        }
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(" = (", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(" == 0 ? ", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }
    if ( xl6CTextEquals(op, "+=") &&
         xl6CgenExprStaticTypeIs(ctx, left, "string") &&
         xl6CgenExprStaticTypeIs(ctx, right, "string") ) {
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(" = xl6_xrt_str_concat(", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }
    if ( xl6CTextEquals(op, "**=") ) {
        if ( !xl6CgenExprIntPowKind(ctx, left) || !xl6CgenExprIntPowKind(ctx, right) ) {
            xl6CgenError(ctx, node, "当前 C 生成 **= 只支持整数表达式");
            return;
        }
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(" = xl6_int_pow_i64(", ctx->out);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(");\n", ctx->out);
        return;
    }

    xl6CgenExpr(ctx, left);
    xl6OutputPrintf(ctx->out, " %s ", op);
    xl6CgenExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenTimeLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* text;
    size_t len;
    const char* body;
    size_t bodyLen;

    text = node != NULL ? node->text : NULL;
    len = text != NULL ? strlen(text) : 0;
    if ( len < 3 || text[0] != '#' || text[len - 1] != '#' ) {
        xl6CgenError(ctx, node, "time literal format must be #yyyy-mm-dd#, #hh:mm:ss# or #yyyy-mm-dd hh:mm:ss#");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    body = text + 1;
    bodyLen = len - 2;
    xl6OutputPrintf(ctx->out, "xrtStrToTime(\"%.*s\", %u)", (int)bodyLen, body, (unsigned int)bodyLen);
}

static void xl6CgenIndexExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    XL6_AstNode* receiverType;

    receiver = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    index = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) ) {
        const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, receiverType);
        const XL6_AstNode* itemType = xl6CgenTypedContainerItemType(ctx, receiverType);
        const char* typeName = concreteType != NULL ? concreteType->text : NULL;
        const char* itemCType = itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue";
        int itemIsFunction = xl6CgenTypeContainsFunctionType(itemType);

        if ( xl6CTextEquals(typeName, "array") ) {
            if ( itemIsFunction ) {
                xl6OutputPuts("xl6_function_from_callable(*(xrt_callable**)xrtTypedArrayGet(", ctx->out);
            } else {
                xl6OutputPuts("(*(", ctx->out);
                xl6OutputPuts(itemCType, ctx->out);
                xl6OutputPuts("*)xrtTypedArrayGet(", ctx->out);
            }
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts(") + 1u))", ctx->out);
        } else if ( xl6CTextEquals(typeName, "list") ) {
            if ( itemIsFunction ) {
                xl6OutputPuts("xl6_function_from_callable(*(xrt_callable**)xrtTypedListGet(", ctx->out);
            } else {
                xl6OutputPuts("(*(", ctx->out);
                xl6OutputPuts(itemCType, ctx->out);
                xl6OutputPuts("*)xrtTypedListGet(", ctx->out);
            }
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts(")))", ctx->out);
        } else if ( xl6CTextEquals(typeName, "dict") ) {
            if ( !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
                xl6CgenError(ctx, index, "dict 索引键必须是 string");
                xl6OutputPuts("xvoCreateNull()", ctx->out);
            } else {
                if ( itemIsFunction ) {
                    xl6OutputPuts("xl6_function_from_callable(*(xrt_callable**)xrtTypedDictGet(", ctx->out);
                } else {
                    xl6OutputPuts("(*(", ctx->out);
                    xl6OutputPuts(itemCType, ctx->out);
                    xl6OutputPuts("*)xrtTypedDictGet(", ctx->out);
                }
                xl6CgenExpr(ctx, receiver);
                xl6OutputPuts(", ", ctx->out);
                xl6CgenExpr(ctx, index);
                xl6OutputPuts(", 0))", ctx->out);
            }
        } else {
            xl6CgenError(ctx, node, "code generation error");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
    } else if ( node != NULL && node->kind == XL6_AST_SAFE_INDEX ) {
        if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6OutputPuts("xl6_xrt_index_safe_get_key(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPutc(')', ctx->out);
        } else if ( receiverType != NULL &&
                    (xl6CgenIsArrayXValueType(receiverType) || xl6CgenIsListXValueType(receiverType) ||
                     xl6CTextEquals(receiverType->text, "var")) ) {
            xl6OutputPuts("xl6_xrt_index_safe_get_i64(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts("))", ctx->out);
        } else {
            xl6CgenError(ctx, node, "当前 C 生成安全索引访问只支�?array/list/dict/var");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
    } else if ( xl6CgenIsArrayXValueType(receiverType) ) {
        xl6OutputPuts("xvoArrayGetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", (unsigned int)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsListXValueType(receiverType) ) {
        xl6OutputPuts("xvoListGetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsDictXValueType(receiverType) ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6CgenError(ctx, index, "dict 索引键必须是 string");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        } else {
            xl6OutputPuts("xvoTableGetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts(", 0)", ctx->out);
        }
    } else {
        xl6CgenError(ctx, node, "当前 C 生成索引访问只支�?array、list �?dict");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    }

    xl6AstFree(receiverType);
}
