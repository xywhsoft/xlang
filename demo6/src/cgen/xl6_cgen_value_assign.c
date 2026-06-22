static int xl6CgenIndexKeyIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* index);
static int xl6CgenIndexKeyIsInteger(XL6_CgenContext* ctx, const XL6_AstNode* index);
static void xl6CgenXValueDictGetExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* receiverName, const XL6_AstNode* index);
static void xl6CgenXValueSafeIndexGetExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* receiverName, const XL6_AstNode* index);
static void xl6CgenTypedDictGetRawExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index);
static void xl6CgenTypedDictSetDynamicKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName);
static void xl6CgenXValueDictSetDynamicKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName);
static void xl6CgenXValueIndexSetI64Stmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName);
static void xl6CgenXValueIndexSetDynamicKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName);
static void xl6CgenIndexAssignTemp(XL6_CgenContext* ctx, const XL6_AstNode* left, const char* tempName)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    XL6_AstNode* receiverType;

    receiver = left != NULL && left->childCount > 0 ? left->children[0] : NULL;
    index = left != NULL && left->childCount > 1 ? left->children[1] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) && xl6CgenIsTypedContainerType(ctx, receiverType) ) {
        const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, receiverType);
        const char* typeName = concreteType != NULL ? concreteType->text : NULL;

        if ( xl6CTextEquals(typeName, "dict") && !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            if ( xl6CgenIndexKeyIsXValue(ctx, index) ) {
                xl6CgenTypedDictSetDynamicKeyStmt(ctx, receiver, index, tempName);
            } else {
                xl6CgenIndent(ctx);
                xl6CgenError(ctx, index, "dict index key must be string or var string");
                xl6OutputPuts("if (!false) {\n", ctx->out);
                ctx->indent += 1;
                xl6CgenIndent(ctx);
                xl6OutputPuts("exit(3);\n", ctx->out);
                ctx->indent -= 1;
                xl6CgenIndent(ctx);
                xl6OutputPuts("}\n", ctx->out);
            }
            xl6AstFree(receiverType);
            return;
        }

        xl6CgenIndent(ctx);
        xl6OutputPuts("if (!", ctx->out);
        if ( xl6CTextEquals(typeName, "array") ) {
            xl6OutputPuts("xrtTypedArraySetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPrintf(ctx->out, ") + 1u, %s)", tempName != NULL ? tempName : "0");
        } else if ( xl6CTextEquals(typeName, "list") ) {
            xl6OutputPuts("xrtTypedListSetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPrintf(ctx->out, "), %s)", tempName != NULL ? tempName : "0");
        } else if ( xl6CTextEquals(typeName, "dict") ) {
            xl6OutputPuts("xrtTypedDictSetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPrintf(ctx->out, ", 0, %s)", tempName != NULL ? tempName : "0");
        } else {
            xl6CgenError(ctx, left, "code generation error");
            xl6OutputPuts("false", ctx->out);
        }
        xl6OutputPuts(") {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("exit(3);\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        if ( tempName != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName);
        }
        xl6AstFree(receiverType);
        return;
    }

    if ( xl6CgenIsDictXValueType(receiverType) && !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
        if ( xl6CgenIndexKeyIsXValue(ctx, index) ) {
            xl6CgenXValueDictSetDynamicKeyStmt(ctx, receiver, index, tempName);
        } else {
            xl6CgenIndent(ctx);
            xl6CgenError(ctx, index, "dict index key must be string or var string");
            xl6OutputPuts("if (false) {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
        xl6AstFree(receiverType);
        return;
    }

    if ( receiverType != NULL && xl6CTextEquals(receiverType->text, "var") ) {
        if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts("if ( !xvoTableSetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPrintf(ctx->out, ", 0, %s, true) && %s != 0 ) {\n", tempName != NULL ? tempName : "0", tempName != NULL ? tempName : "0");
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        } else if ( xl6CgenIndexKeyIsXValue(ctx, index) ) {
            xl6CgenXValueIndexSetDynamicKeyStmt(ctx, receiver, index, tempName);
        } else if ( xl6CgenIndexKeyIsInteger(ctx, index) ) {
            xl6CgenXValueIndexSetI64Stmt(ctx, receiver, index, tempName);
        } else {
            xl6CgenIndent(ctx);
            xl6CgenError(ctx, index, "var container index key must be int, string or var");
            xl6OutputPuts("if (false) {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
        xl6AstFree(receiverType);
        return;
    }
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
        xl6OutputPuts("xvoTableSetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPrintf(ctx->out, ", 0, %s, true)", tempName != NULL ? tempName : "0");
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
        xl6CgenXValueScalarGetExpr(ctx, "long long", "xvoGetInt", expr);
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
        xl6CgenXValueScalarGetExpr(ctx, "double", "xvoGetFloat", expr);
    } else {
        xl6OutputPuts("(double)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    }
    xl6AstFree(typeNode);
}
static int xl6CgenTypedIndexCompoundAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* itemType;
    const char* itemTypeName;
    const char* binaryOp;
    int id;
    char currentName[64];
    char rightName[64];
    char tempName[64];

    receiver = left != NULL && left->childCount > 0 ? left->children[0] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenExprUsesTypedContainerStorage(ctx, receiver) || !xl6CgenIsTypedContainerType(ctx, receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }

    itemType = xl6CgenCloneExprTypeForInference(ctx, left);
    itemTypeName = itemType != NULL ? itemType->text : NULL;
    id = ctx->tempId++;
    snprintf(currentName, sizeof(currentName), "__xl6_typed_index_current_%d", id);
    snprintf(rightName, sizeof(rightName), "__xl6_typed_index_right_%d", id);
    snprintf(tempName, sizeof(tempName), "__xl6_typed_index_assign_%d", id);

    if ( xl6CTextEquals(itemTypeName, "string") ) {
        if ( !xl6CTextEquals(op, "+=") ) {
            xl6CgenError(ctx, left, "typed string index compound assignment only supports +=");
            xl6AstFree(itemType);
            xl6AstFree(receiverType);
            return 1;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "const char* %s = ", currentName);
        xl6CgenIndexExpr(ctx, left);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "const char* %s = ", rightName);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateText((void*)xl6_xrt_str_concat(%s, %s), 0, true);\n",
                tempName, currentName, rightName);
        xl6CgenIndexAssignTemp(ctx, left, tempName);
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( !xl6CgenIsIntegerTypeName(itemTypeName) && !xl6CgenIsFloatTypeName(itemTypeName) ) {
        xl6CgenError(ctx, left, "typed index compound assignment requires numeric or string item type");
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(op, "?=") ) {
        xl6CgenError(ctx, left, "typed index ?= is not supported");
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CTextEquals(op, "**=") ) {
        if ( !xl6CgenIsIntegerTypeName(itemTypeName) || !xl6CgenExprIntPowKind(ctx, right) ) {
            xl6CgenError(ctx, left, "typed index **= only supports integer item and integer expression");
            xl6AstFree(itemType);
            xl6AstFree(receiverType);
            return 1;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = (long long)(", currentName);
        xl6CgenIndexExpr(ctx, left);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = (long long)(", rightName);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateInt(xl6_int_pow_i64(%s, %s));\n", tempName, currentName, rightName);
        xl6CgenIndexAssignTemp(ctx, left, tempName);
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return 1;
    }

    binaryOp = xl6CgenCompoundBinaryOp(op);
    if ( binaryOp == NULL ) {
        xl6CgenError(ctx, left, "unsupported typed index compound assignment operator");
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return 1;
    }
    if ( xl6CTextEquals(op, "%=") && xl6CgenIsFloatTypeName(itemTypeName) ) {
        xl6CgenError(ctx, left, "typed float index %= is not supported");
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( xl6CgenIsFloatTypeName(itemTypeName) || xl6CgenExprTypeIsFloat(ctx, right) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "double %s = (double)(", currentName);
        xl6CgenIndexExpr(ctx, left);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "double %s = (double)(", rightName);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateFloat(%s %s %s);\n", tempName, currentName, binaryOp, rightName);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = (long long)(", currentName);
        xl6CgenIndexExpr(ctx, left);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = (long long)(", rightName);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateInt(%s %s %s);\n", tempName, currentName, binaryOp, rightName);
    }
    xl6CgenIndexAssignTemp(ctx, left, tempName);

    xl6AstFree(itemType);
    xl6AstFree(receiverType);
    return 1;
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
    if ( xl6CgenTypedIndexCompoundAssignStmt(ctx, left, op, right) ) {
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
        XL6_AstNode* fieldType;
        XL6_AstNode* fieldCustom;

        ctorCustom = NULL;
        fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, left);
        if ( xl6CgenIsFunctionType(fieldType) ) {
            int id = ctx->tempId++;
            char tempName[64];

            snprintf(tempName, sizeof(tempName), "__xl6_function_field_assign_%d", id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "XL6_Function %s = ", tempName);
            xl6CgenFunctionValueExpr(ctx, right, fieldType);
            xl6OutputPuts(";\n", ctx->out);
            if ( xl6CgenExprIsFunctionBorrow(ctx, right) ) {
                xl6CgenRetainFunctionValue(ctx, tempName);
            }
            xl6CgenIndent(ctx);
            xl6OutputPuts("if (", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".release != 0 && ", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".env != 0) {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".release(", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".env);\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".env = 0;\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".release = 0;\n", ctx->out);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPrintf(ctx->out, " = %s;\n", tempName);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenCallIsCustomCtor(ctx, right, &ctorCustom) ) {
            fieldCustom = fieldType != NULL ? xl6CgenFindCustom(ctx, fieldType->text) : NULL;
            if ( fieldCustom == NULL ) {
                xl6AstFree(fieldType);
                xl6CgenError(ctx, left, "custom 构造只能赋给同类型 custom 字段");
                return;
            }
            if ( !xl6CTextEquals(fieldCustom->text, ctorCustom->text) ) {
                xl6CgenError(ctx, right, "code generation error");
                xl6AstFree(fieldType);
                return;
            }
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".handle = ", ctx->out);
            xl6CgenCustomCtorValue(ctx, ctorCustom, right);
            xl6OutputPuts(";\n", ctx->out);
            xl6AstFree(fieldType);
            return;
        }
        xl6AstFree(fieldType);
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME ) {
        const XL6_AstNode* targetType = xl6CgenFindVarType(ctx, left->text);
        XL6_AstNode* classNode = xl6CgenClassNodeForType(ctx, targetType);

        if ( classNode != NULL && xl6CgenClassNeedsDestroy(ctx, classNode) ) {
            int id = ctx->tempId++;
            char cName[512];
            char tempName[64];

            xl6CgenCIdentText(cName, sizeof(cName), left->text);
            snprintf(tempName, sizeof(tempName), "__xl6_class_assign_%d", id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, targetType), tempName);
            xl6CgenExpr(ctx, right);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenRetainClassFunctionFieldsForType(ctx, targetType, tempName);
            xl6CgenReleaseClassFunctionFieldsForType(ctx, targetType, cName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
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

static void xl6CgenXValueDictGetExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* receiverName, const XL6_AstNode* index)
{
    int id;

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

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_dict_key_%d = ", id);
    xl6CgenXValueFromExpr(ctx, index);
    xl6OutputPrintf(ctx->out, "; xvalue __xl6_dict_result_%d = (__xl6_dict_key_%d != 0 && xvoType(__xl6_dict_key_%d) == XL6_XVO_DT_TEXT) ? xvoTableGetValue(", id, id, id);
    xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
    xl6OutputPrintf(ctx->out, ", xvoGetText(__xl6_dict_key_%d), 0) : xvoTableGetValue(0, \"\", 0); if (__xl6_dict_key_%d != 0) { xvoUnref(__xl6_dict_key_%d); } __xl6_dict_result_%d; })", id, id, id, id);
}

static void xl6CgenXValueSafeIndexGetExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* receiverName, const XL6_AstNode* index)
{
    int id;

    if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
        xl6OutputPuts("xl6_xrt_index_safe_get_key(", ctx->out);
        xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, index);
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

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_index_key_%d = ", id);
    xl6CgenXValueFromExpr(ctx, index);
    xl6OutputPrintf(ctx->out, "; xvalue __xl6_index_value_%d = (__xl6_index_key_%d != 0 && xvoType(__xl6_index_key_%d) == XL6_XVO_DT_TEXT) ? xl6_xrt_index_safe_get_key(", id, id, id);
    xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
    xl6OutputPrintf(ctx->out, ", xvoGetText(__xl6_index_key_%d)) : xl6_xrt_index_safe_get_i64(", id);
    xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
    xl6OutputPrintf(ctx->out, ", (__xl6_index_key_%d != 0 ? (long long)xvoGetInt(__xl6_index_key_%d) : 0)); if (__xl6_index_key_%d != 0) { xvoUnref(__xl6_index_key_%d); } __xl6_index_value_%d; })", id, id, id, id, id);
}
static void xl6CgenTypedDictGetRawExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index)
{
    int id;

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

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_dict_key_%d = ", id);
    xl6CgenXValueFromExpr(ctx, index);
    xl6OutputPrintf(ctx->out, "; void* __xl6_dict_ptr_%d = (__xl6_dict_key_%d != 0 && xvoType(__xl6_dict_key_%d) == XL6_XVO_DT_TEXT) ? xrtTypedDictGet(", id, id, id);
    xl6CgenExpr(ctx, receiver);
    xl6OutputPrintf(ctx->out, ", xvoGetText(__xl6_dict_key_%d), 0) : 0; if (__xl6_dict_key_%d != 0) { xvoUnref(__xl6_dict_key_%d); } __xl6_dict_ptr_%d; })", id, id, id, id);
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
    xl6CgenIndent(ctx);
    xl6OutputPuts("exit(3);\n", ctx->out);
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
    xl6OutputPrintf(ctx->out, "if (!((__xl6_index_type_%d == XL6_XVO_DT_ARRAY && xvoArraySetValue(__xl6_index_values_%d, (unsigned int)(", id, id);
    xl6CgenExpr(ctx, index);
    xl6OutputPrintf(ctx->out, "), %s, true)) || (__xl6_index_type_%d == XL6_XVO_DT_LIST && xvoListSetValue(__xl6_index_values_%d, (long long)(", tempName != NULL ? tempName : "0", id, id);
    xl6CgenExpr(ctx, index);
    xl6OutputPrintf(ctx->out, "), %s, true))) && %s != 0) {\n", tempName != NULL ? tempName : "0", tempName != NULL ? tempName : "0");
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
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
    xl6OutputPrintf(ctx->out, "if (__xl6_index_key_%d != 0 && xvoType(__xl6_index_key_%d) == XL6_XVO_DT_TEXT) {\n", id, id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_index_ok_%d = __xl6_index_type_%d == XL6_XVO_DT_TABLE && xvoTableSetValue(__xl6_index_values_%d, xvoGetText(__xl6_index_key_%d), 0, %s, true);\n", id, id, id, id, tempName != NULL ? tempName : "0");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "long long __xl6_index_i_%d = (__xl6_index_key_%d != 0 ? (long long)xvoGetInt(__xl6_index_key_%d) : 0);\n", id, id, id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_index_ok_%d = (__xl6_index_type_%d == XL6_XVO_DT_ARRAY && xvoArraySetValue(__xl6_index_values_%d, (unsigned int)__xl6_index_i_%d, %s, true)) || (__xl6_index_type_%d == XL6_XVO_DT_LIST && xvoListSetValue(__xl6_index_values_%d, __xl6_index_i_%d, %s, true));\n", id, id, id, id, tempName != NULL ? tempName : "0", id, id, id, tempName != NULL ? tempName : "0");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!__xl6_index_ok_%d && %s != 0) {\n", id, tempName != NULL ? tempName : "0");
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_index_key_%d != 0) { xvoUnref(__xl6_index_key_%d); }\n", id, id);
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
        xl6OutputPuts("xvoArrayGetValue(", ctx->out);
        xl6CgenEmitIndexReceiverExpr(ctx, receiver, receiverName);
        xl6OutputPuts(", (unsigned int)(", ctx->out);
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

        if ( node != NULL && node->kind == XL6_AST_SAFE_INDEX ) {
            xl6OutputPuts("xl6_xrt_typed_box_or_null(", ctx->out);
            xl6CgenEmitXrtTypeDescRef(ctx, itemType);
            xl6OutputPuts(", ", ctx->out);
            if ( xl6CTextEquals(typeName, "array") ) {
                xl6OutputPuts("xrtTypedArrayGet(", ctx->out);
                xl6CgenExpr(ctx, receiver);
                xl6OutputPuts(", (unsigned int)(", ctx->out);
                xl6CgenExpr(ctx, index);
                xl6OutputPuts(") + 1u)", ctx->out);
            } else if ( xl6CTextEquals(typeName, "list") ) {
                xl6OutputPuts("xrtTypedListGet(", ctx->out);
                xl6CgenExpr(ctx, receiver);
                xl6OutputPuts(", (long long)(", ctx->out);
                xl6CgenExpr(ctx, index);
                xl6OutputPuts("))", ctx->out);
            } else if ( xl6CTextEquals(typeName, "dict") ) {
                xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
            } else {
                xl6CgenError(ctx, node, "code generation error");
                xl6OutputPuts("0", ctx->out);
            }
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(typeName, "array") ) {
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
            if ( itemIsFunction ) {
                xl6OutputPuts("xl6_function_from_callable(*(xrt_callable**)", ctx->out);
                xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
                xl6OutputPuts(")", ctx->out);
            } else {
                xl6OutputPuts("(*(", ctx->out);
                xl6OutputPuts(itemCType, ctx->out);
                xl6OutputPuts("*)", ctx->out);
                xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
                xl6OutputPuts(")", ctx->out);
            }
        } else {
            xl6CgenError(ctx, node, "code generation error");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
    } else if ( !xl6CgenExprIsXValueBorrow(ctx, receiver) && xl6CgenDynamicIndexReceiverCanOwnResult(node, index, receiverType) ) {
        int id = ctx->tempId++;
        char receiverName[64];
        snprintf(receiverName, sizeof(receiverName), "__xl6_index_values_%d", id);
        xl6OutputPrintf(ctx->out, "({ xvalue %s = ", receiverName);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPrintf(ctx->out, "; xvalue __xl6_index_result_%d = xl6_xrt_retain(", id);
        xl6CgenDynamicIndexBorrowExpr(ctx, node, receiver, receiverName, index, receiverType);
        xl6OutputPrintf(ctx->out, "); if (%s != 0) { xvoUnref(%s); } __xl6_index_result_%d; })", receiverName, receiverName, id);
    } else {
        xl6CgenDynamicIndexBorrowExpr(ctx, node, receiver, NULL, index, receiverType);
    }
    xl6AstFree(receiverType);
}
