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

static const XL6_AstNode* xl6CgenNameAssignTargetType(XL6_CgenContext* ctx, const char* name);
static void xl6CgenNameAssignTargetPath(XL6_CgenContext* ctx, const char* name, char* buffer, size_t capacity);

static int xl6CgenClassProtocolCompoundAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right)
{
    const char* binaryOp;
    const char* protocolName;
    const XL6_AstNode* classNode;
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    const XL6_AstNode* argType;
    XL6_AstNode* receiverType;
    XL6_AstNode* protocolCall;
    const XL6_AstNode* targetType;
    char cName[512];
    char tempName[64];
    int id;

    if ( ctx == NULL || left == NULL || left->kind != XL6_AST_NAME || right == NULL ) {
        return 0;
    }
    binaryOp = xl6CgenCompoundBinaryOp(op);
    protocolName = binaryOp != NULL ? xl6CgenBinaryProtocolName(binaryOp) : NULL;
    if ( protocolName == NULL ) {
        return 0;
    }

    receiverType = NULL;
    protocolCall = xl6CgenProtocolTempCallNew(left);
    if ( protocolCall == NULL || !xl6CgenProtocolTempCallAddArg(protocolCall, right) ||
         !xl6CgenResolveClassProtocolReceiverForCall(ctx, left, protocolName, protocolCall, &classNode, &ownerClass, &method, &receiverType) ) {
        xl6CgenProtocolTempCallFree(protocolCall);
        return 0;
    }
    xl6CgenProtocolTempCallFree(protocolCall);

    targetType = xl6CgenNameAssignTargetType(ctx, left->text);
    xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_class_compound_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts(xl6CgenTypeName(ctx, targetType), ctx->out);
    xl6OutputPrintf(ctx->out, " %s = ", tempName);
    argType = xl6CgenClassProtocolParamType(method, 0);
    if ( xl6CgenClassProtocolCallPrefix(ctx, left, classNode, ownerClass, method, receiverType) ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenClassProtocolExprArg(ctx, argType, right);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6OutputPuts("{0}", ctx->out);
    }
    xl6OutputPuts(";\n", ctx->out);
    xl6AstFree(receiverType);

    xl6CgenReleaseClassFunctionFieldsForType(ctx, targetType, cName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
    return 1;
}

static const XL6_AstNode* xl6CgenNameAssignTargetType(XL6_CgenContext* ctx, const char* name)
{
    const XL6_AstNode* targetType;
    const XL6_AstNode* field;

    targetType = xl6CgenFindVarType(ctx, name);
    if ( targetType != NULL ) {
        return targetType;
    }
    field = xl6CgenFindImplicitSelfFieldNode(ctx, name);
    return field != NULL && field->childCount > 0 ? field->children[0] : NULL;
}

static void xl6CgenNameAssignTargetPath(XL6_CgenContext* ctx, const char* name, char* buffer, size_t capacity)
{
    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    if ( xl6CgenImplicitSelfFieldPath(ctx, name, buffer, capacity) ) {
        return;
    }
    xl6CgenCIdentText(buffer, capacity, name);
}

static int xl6CgenTypedIndexCompoundAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const char* op,
    const XL6_AstNode* right)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    const XL6_AstNode* receiverConcrete;
    XL6_AstNode* receiverType;
    XL6_AstNode* itemType;
    const char* itemTypeName;
    const char* receiverKind;
    const char* binaryOp;
    int id;
    char currentName[64];
    char rightName[64];
    char tempName[64];

    receiver = left != NULL && left->childCount > 0 ? left->children[0] : NULL;
    index = left != NULL && left->childCount > 1 ? left->children[1] : NULL;
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
    receiverConcrete = xl6CgenTypedContainerConcreteType(ctx, receiverType);
    receiverKind = receiverConcrete != NULL ? receiverConcrete->text : NULL;

    if ( xl6CTextEquals(op, "?=") ) {
        if ( !xl6CTextEquals(receiverKind, "array") &&
             !xl6CTextEquals(receiverKind, "list") &&
             !xl6CTextEquals(receiverKind, "dict") ) {
            xl6CgenError(ctx, left, "typed index ?= requires array, list or dict");
            xl6AstFree(itemType);
            xl6AstFree(receiverType);
            return 1;
        }
        if ( xl6CTextEquals(receiverKind, "dict") &&
             !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6CgenError(ctx, index, "typed dict ?= key must be string");
            xl6AstFree(itemType);
            xl6AstFree(receiverType);
            return 1;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xl6_xrt_typed_box_or_null(", currentName);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        if ( xl6CTextEquals(receiverKind, "array") ) {
            xl6OutputPuts("xl6_xrt_typed_array_get_at(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (unsigned int)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CTextEquals(receiverKind, "list") ) {
            xl6OutputPuts("xrtTypedListGet(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts("))", ctx->out);
        } else {
            xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
        }
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0 || xvoType(%s) == XL6_XVO_DT_NULL) {\n", currentName, currentName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); %s = 0; }\n", currentName, currentName, currentName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = ", tempName);
        xl6CgenXValueFromExpr(ctx, right);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndexAssignTemp(ctx, left, tempName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", currentName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6AstFree(itemType);
        xl6AstFree(receiverType);
        return 1;
    }

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
        xl6CgenXValueAsI64Expr(ctx, right);
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
        snprintf(tempName, sizeof(tempName), "typed index compound operator is not supported: %s", op != NULL ? op : "<missing>");
        xl6CgenError(ctx, left, tempName);
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
        xl6CgenXValueAsF64Expr(ctx, right);
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
        xl6CgenXValueAsI64Expr(ctx, right);
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

    /*
        xvoArrayGetValue/xvoListGetValue/xvoTableGetValue 返回容器内借用值。
        current 只用于本语句内读取旧值，不能 xvoUnref，否则会误减容器元素引用。
    */
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

static int xl6CgenDynamicMemberReceiverTypeAllowed(XL6_CgenContext* ctx, const XL6_AstNode* receiverType)
{
    return xl6CgenMemberKeyReceiverIsDynamic(ctx, receiverType);
}

static int xl6CgenMemberAssignIsDynamicTarget(XL6_CgenContext* ctx, const XL6_AstNode* left)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    int result;

    if ( left == NULL || left->kind != XL6_AST_MEMBER || left->text == NULL || left->childCount == 0 ) {
        return 0;
    }
    receiver = left->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    result = xl6CgenIsXValueType(receiverType);
    xl6AstFree(receiverType);
    return result;
}

static int xl6CgenDynamicMemberAssignStmt(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    int id;
    int receiverBorrow;
    char receiverName[64];
    char tempName[64];

    if ( left == NULL || left->kind != XL6_AST_MEMBER || left->text == NULL || left->childCount == 0 ) {
        return 0;
    }

    receiver = left->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( !xl6CgenIsXValueType(receiverType) ) {
        xl6AstFree(receiverType);
        return 0;
    }
    if ( xl6CTextEquals(left->text, "length") ) {
        xl6CgenError(ctx, left, "member length is read-only");
        xl6AstFree(receiverType);
        return 1;
    }
    if ( !xl6CgenDynamicMemberReceiverTypeAllowed(ctx, receiverType) ) {
        xl6CgenError(ctx, left, "dynamic member assignment receiver must be var or dynamic dict");
        xl6AstFree(receiverType);
        return 1;
    }

    id = ctx->tempId++;
    receiverBorrow = xl6CgenExprIsXValueBorrow(ctx, receiver);
    receiverName[0] = '\0';
    snprintf(tempName, sizeof(tempName), "__xl6_member_assign_%d", id);
    if ( !receiverBorrow ) {
        snprintf(receiverName, sizeof(receiverName), "__xl6_member_receiver_%d", id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = ", receiverName);
        /*
            嵌套动态字段赋值需要先把 receiver 固化为 owned xvalue。
            例如 nested.inner.name = 1：inner 是从 nested 取出的字典对象，
            写入 name 后立即释放这个临时引用，底层对象仍由 nested 持有。
        */
        xl6CgenXValueFromExpr(ctx, receiver);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = ", tempName);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( !xl6_record_field_set_borrow(", ctx->out);
    if ( receiverBorrow ) {
        xl6CgenExpr(ctx, receiver);
    } else {
        xl6OutputPuts(receiverName, ctx->out);
    }
    xl6OutputPuts(", ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, left->text);
    xl6OutputPrintf(ctx->out, ", %s) ) {\n", tempName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("/* 动态成员写入失败沿用旧语义：释放右值，不额外抛错。 */\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if ( %s != 0 ) { xvoUnref(%s); }\n", tempName, tempName);
    if ( !receiverBorrow ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if ( %s != 0 ) { xvoUnref(%s); }\n", receiverName, receiverName);
    }
    xl6AstFree(receiverType);
    return 1;
}
static int xl6CgenAssignStaticBoolLikeType(const XL6_AstNode* typeNode)
{
    const char* name;

    if ( typeNode == NULL || typeNode->text == NULL ) {
        return 0;
    }

    name = typeNode->text;
    return xl6CgenIsBoolTypeName(name) ||
           xl6CTextEquals(name, "int") ||
           xl6CTextEquals(name, "int8") ||
           xl6CTextEquals(name, "uint8") ||
           xl6CTextEquals(name, "int16") ||
           xl6CTextEquals(name, "uint16") ||
           xl6CTextEquals(name, "int32") ||
           xl6CTextEquals(name, "uint32") ||
           xl6CTextEquals(name, "int64") ||
           xl6CTextEquals(name, "uint64");
}

static void xl6CgenAssignEmitXValueTempCleanup(XL6_CgenContext* ctx, const char* prefix, int id)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_%s_left_%d != 0) {\n", prefix, id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_%s_left_%d);\n", prefix, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_%s_right_%d != 0) {\n", prefix, id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_%s_right_%d);\n", prefix, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static int xl6CgenAssignTryXValueBinaryStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* right,
    int id)
{
    const XL6_AstNode* leftExpr;
    const XL6_AstNode* rightExpr;

    if ( ctx == NULL || targetType == NULL ||
         right == NULL || right->kind != XL6_AST_BINARY || right->childCount < 2 ||
         !xl6CgenBinaryArithmeticIsXValue(ctx, right) ) {
        return 0;
    }

    leftExpr = right->children[0];
    rightExpr = right->children[1];

    xl6OutputPrintf(ctx->out, "xvalue __xl6_assign_%d = 0;\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_bin_left_%d = ", id);
    xl6CgenXValueFromExprForTarget(ctx, targetType, leftExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_bin_right_%d = ", id);
    xl6CgenXValueFromExprForTarget(ctx, targetType, rightExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "__xl6_assign_%d = xl6_xrt_basic_binary(__xl6_bin_left_%d, __xl6_bin_right_%d, %d);\n",
                    id,
                    id,
                    id,
                    xl6CgenXValueBinaryMode(right->text));
    xl6CgenAssignEmitXValueTempCleanup(ctx, "bin", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenAssignTryXValueConditionalStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* right,
    int id)
{
    const XL6_AstNode* cond;
    const XL6_AstNode* thenExpr;
    const XL6_AstNode* elseExpr;

    if ( ctx == NULL || targetType == NULL ||
         right == NULL || right->kind != XL6_AST_CONDITIONAL || right->childCount < 3 ||
         !xl6CgenConditionalIsXValue(ctx, right) ) {
        return 0;
    }

    cond = right->children[0];
    thenExpr = right->children[1];
    elseExpr = right->children[2];

    xl6OutputPrintf(ctx->out, "xvalue __xl6_assign_%d = 0;\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("if (", ctx->out);
    xl6CgenConditionExpr(ctx, cond);
    xl6OutputPuts(") {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_assign_%d = ", id);
    xl6CgenXValueFromExprForTarget(ctx, targetType, thenExpr);
    xl6OutputPuts(";\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_assign_%d = ", id);
    xl6CgenXValueFromExprForTarget(ctx, targetType, elseExpr);
    xl6OutputPuts(";\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenAssignTryStaticXValueCompareStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const char* cType,
    const char* cName,
    const XL6_AstNode* right)
{
    const XL6_AstNode* leftExpr;
    const XL6_AstNode* rightExpr;
    int id;
    int equalMode;
    int negate;

    if ( ctx == NULL || targetType == NULL || cType == NULL || cName == NULL ||
         right == NULL || right->kind != XL6_AST_BINARY || right->childCount < 2 ||
         !xl6CgenAssignStaticBoolLikeType(targetType) ||
         !xl6CgenBinaryCompareIsXValue(ctx, right) ) {
        return 0;
    }

    leftExpr = right->children[0];
    rightExpr = right->children[1];
    id = ctx->tempId++;
    equalMode = xl6CTextEquals(right->text, "==") || xl6CTextEquals(right->text, "!=");
    negate = xl6CTextEquals(right->text, "!=");

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_cmp_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, leftExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_cmp_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, rightExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    if ( equalMode ) {
        xl6OutputPrintf(ctx->out,
                        "int __xl6_cmp_result_%d = xl6_xrt_basic_equal(__xl6_cmp_left_%d, __xl6_cmp_right_%d);\n",
                        id,
                        id,
                        id);
    } else {
        xl6OutputPrintf(ctx->out,
                        "int __xl6_cmp_result_%d = xl6_xrt_basic_order(__xl6_cmp_left_%d, __xl6_cmp_right_%d, %d);\n",
                        id,
                        id,
                        id,
                        xl6CgenXValueOrderMode(right->text));
    }
    xl6CgenAssignEmitXValueTempCleanup(ctx, "cmp", id);
    xl6CgenIndent(ctx);
    if ( negate ) {
        xl6OutputPrintf(ctx->out, "%s = (%s)(!__xl6_cmp_result_%d);\n", cName, cType, id);
    } else {
        xl6OutputPrintf(ctx->out, "%s = (%s)(__xl6_cmp_result_%d);\n", cName, cType, id);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenAssignTryStaticXValueApproxStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const char* cType,
    const char* cName,
    const XL6_AstNode* right)
{
    const XL6_AstNode* leftExpr;
    const XL6_AstNode* rightExpr;
    int id;

    if ( ctx == NULL || targetType == NULL || cType == NULL || cName == NULL ||
         right == NULL || right->kind != XL6_AST_BINARY || right->childCount < 2 ||
         !xl6CgenAssignStaticBoolLikeType(targetType) ||
         !xl6CgenBinaryApproxIsXValue(ctx, right) ) {
        return 0;
    }

    leftExpr = right->children[0];
    rightExpr = right->children[1];
    id = ctx->tempId++;

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_approx_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, leftExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_approx_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, rightExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "int __xl6_approx_result_%d = xl6_xrt_basic_approx(__xl6_approx_left_%d, __xl6_approx_right_%d);\n",
                    id,
                    id,
                    id);
    xl6CgenAssignEmitXValueTempCleanup(ctx, "approx", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = (%s)(__xl6_approx_result_%d);\n", cName, cType, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenXValueMemberAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const XL6_AstNode* right,
    const XL6_AstNode* targetType)
{
    int id;
    char targetPath[512];

    if ( ctx == NULL || left == NULL || right == NULL || !xl6CgenIsXValueType(targetType) ) {
        return 0;
    }
        if ( !xl6CgenSimpleAssignTargetPath(ctx, left, targetPath, sizeof(targetPath)) ) {
        xl6CgenError(ctx, left, "var member assignment requires a direct field target");
        return 1;
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    if ( !xl6CgenAssignTryXValueConditionalStmt(ctx, targetType, right, id) &&
         !xl6CgenAssignTryXValueBinaryStmt(ctx, targetType, right, id) ) {
        xl6OutputPrintf(ctx->out, "xvalue __xl6_assign_%d = ", id);
        xl6CgenXValueFromExprForTarget(ctx, targetType, right);
        xl6OutputPuts(";\n", ctx->out);
    }

    /* var 字段拥有 xvalue 引用，替换前必须释放旧值。 */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", targetPath);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", targetPath);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = __xl6_assign_%d;\n", targetPath, id);
    return 1;
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
        xl6CgenError(ctx, node, "assignment target must be name, member or index");
        return;
    }
    if ( left->kind == XL6_AST_INDEX ) {
        if ( xl6CTextEquals(op, "=") && xl6CgenClassPropertyIndexSetStmt(ctx, left, right) ) {
            return;
        }
        if ( xl6CTextEquals(op, "=") && xl6CgenClassPropertyIndexHasGetter(ctx, left) ) {
            xl6CgenError(ctx, left, "class property index assignment requires let");
            return;
        }
        if ( xl6CTextEquals(op, "=") && xl6CgenClassProtocolIndexSetStmt(ctx, left, right) ) {
            return;
        }
        if ( xl6CTextEquals(op, "=") && xl6CgenClassProtocolIndexHasGetter(ctx, left) ) {
            xl6CgenError(ctx, left, "class protocol index assignment requires __setItem__");
            return;
        }
        xl6CgenIndexCompoundAssignStmt(ctx, left, op, right);
        return;
    }
    if ( xl6CgenTypedContainerCompoundAssignStmt(ctx, left, op, right) ) {
        return;
    }
    if ( !xl6CTextEquals(op, "=") && xl6CgenClassProtocolCompoundAssignStmt(ctx, left, op, right) ) {
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* ctorCustom;
        XL6_AstNode* fieldType;
        XL6_AstNode* fieldCustom;

        ctorCustom = NULL;
        if ( xl6CgenClassPropertyMemberSetStmt(ctx, left, right) ) {
            return;
        }
        if ( xl6CgenClassPropertyMemberHasGetter(ctx, left) ) {
            xl6CgenError(ctx, left, "class property assignment requires let");
            return;
        }
        if ( xl6CgenClassProtocolMemberSetStmt(ctx, left, right) ) {
            return;
        }
        if ( xl6CgenClassProtocolMemberHasGetter(ctx, left) ) {
            xl6CgenError(ctx, left, "class protocol member assignment requires __setField__");
            return;
        }
        fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, left);
        if ( fieldType == NULL && xl6CgenDynamicMemberAssignStmt(ctx, left, right) ) {
            return;
        }
        if ( xl6CgenTypedContainerMemberAssignStmt(ctx, left, right, fieldType) ) {
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenXValueMemberAssignStmt(ctx, left, right, fieldType) ) {
            xl6AstFree(fieldType);
            return;
        }
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
        if ( xl6CgenCallIsNativeBackedCtor(ctx, right, &ctorCustom) ) {
            fieldCustom = fieldType != NULL ? xl6CgenFindNativeBackedClass(ctx, fieldType->text) : NULL;
            if ( fieldCustom == NULL ) {
                xl6AstFree(fieldType);
                xl6CgenError(ctx, left, "native-backed class constructor can only assign to same-type native-backed class field");
                return;
            }
            if ( !xl6CTextEquals(fieldCustom->text, ctorCustom->text) ) {
                xl6CgenError(ctx, right, "native-backed class constructor type does not match target field type");
                xl6AstFree(fieldType);
                return;
            }
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".handle = ", ctx->out);
            xl6CgenNativeBackedCtorValue(ctx, ctorCustom, right);
            xl6OutputPuts(";\n", ctx->out);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenTryClassCtorAssignTarget(ctx, left, right, fieldType) ) {
            xl6AstFree(fieldType);
            return;
        }
        xl6AstFree(fieldType);
    }
    if ( left->kind == XL6_AST_MEMBER && xl6CgenMemberAssignIsDynamicTarget(ctx, left) ) {
        xl6CgenError(ctx, node, "dynamic member assignment only supports =");
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsTypedContainerType(ctx, xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        const XL6_AstNode* literal = xl6CgenTypedContainerInitLiteral(ctx, targetType, right);
        XL6_AstNode* targetCanonical = xl6CgenCloneCanonicalTypeNode(ctx, targetType);
        XL6_AstNode* rightType = literal == NULL ? xl6CgenCloneExprTypeForInference(ctx, right) : NULL;
        int moveReturn = literal == NULL && xl6CgenTypedContainerCanMoveFromExpr(ctx, targetType, right);
        unsigned int targetFlags = xl6CgenFindTrackedVarFlags(ctx, left->text);
        int targetBorrowed = (targetFlags & XL6_CGEN_VAR_BORROWED) != 0;
        int id = ctx->tempId++;
        char cName[512];
        char tempName[64];
        char memberTempName[64];
        XL6_AstNode* memberFunctionType;

        if ( literal == NULL &&
             (targetCanonical == NULL || rightType == NULL || !xl6CgenTypeNodeEqual(targetCanonical, rightType)) ) {
            xl6CgenError(ctx, right, "typed container assignment requires the same container type or literal");
            xl6AstFree(targetCanonical);
            xl6AstFree(rightType);
            return;
        }
        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
        snprintf(tempName, sizeof(tempName), "__xl6_typed_container_assign_%d", id);
        memberTempName[0] = '\0';
        memberFunctionType = NULL;
        if ( moveReturn ) {
            memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, right);
            if ( memberFunctionType != NULL ) {
                snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", id);
                xl6CgenEmitMemberFunctionStatementTemp(ctx, right, memberTempName);
            }
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, targetType), tempName);
        if ( literal != NULL ) {
            xl6CgenTypedContainerCreateExpr(ctx, targetType);
        } else if ( moveReturn ) {
            if ( memberFunctionType != NULL ) {
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, right, memberFunctionType, memberTempName);
            } else {
                xl6CgenExpr(ctx, right);
            }
        } else {
            xl6CgenTypedContainerCopyExpr(ctx, targetType, right);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6AstFree(memberFunctionType);
        if ( literal != NULL ) {
            xl6CgenTypedContainerFillFromLiteral(ctx, tempName, targetType, right);
        }
        if ( targetBorrowed ) {
            char ownerName[512];

            xl6CgenTypedParamOwnerName(ownerName, sizeof(ownerName), left->text);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s) {\n", ownerName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, targetType, cName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("} else {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = 1;\n", ownerName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        } else {
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, targetType, cName);
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
        xl6AstFree(targetCanonical);
        xl6AstFree(rightType);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        XL6_AstNode* classNode = xl6CgenClassNodeForType(ctx, targetType);
        int upcastAssign = xl6CgenExprNeedsClassUpcast(ctx, targetType, right, NULL, 0, NULL, NULL);

        if ( classNode != NULL && (upcastAssign || xl6CgenClassNeedsDestroy(ctx, classNode)) ) {
            int id = ctx->tempId++;
            char cName[512];
            char tempName[64];
            char memberTempName[64];
            XL6_AstNode* memberFunctionType;

            xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
            snprintf(tempName, sizeof(tempName), "__xl6_class_assign_%d", id);
            snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", id);
            memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, right);
            if ( memberFunctionType != NULL ) {
                xl6CgenEmitMemberFunctionStatementTemp(ctx, right, memberTempName);
            }
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, targetType), tempName);
            if ( upcastAssign ) {
                xl6CgenClassUpcastExpr(ctx, targetType, right);
            } else if ( memberFunctionType != NULL ) {
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, right, memberFunctionType, memberTempName);
            } else {
                xl6CgenExpr(ctx, right);
            }
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, targetType, tempName, right);
            xl6AstFree(memberFunctionType);
            xl6CgenReleaseClassFunctionFieldsForType(ctx, targetType, cName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
            return;
        }
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsFunctionType(xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];
        char tempName[64];
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);

        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
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
         xl6CgenIsXValueType(xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);

        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));

        xl6CgenIndent(ctx);
        if ( !xl6CgenAssignTryXValueConditionalStmt(ctx, targetType, right, id) &&
             !xl6CgenAssignTryXValueBinaryStmt(ctx, targetType, right, id) ) {
            xl6OutputPrintf(ctx->out, "xvalue __xl6_assign_%d = ", id);
            xl6CgenXValueFromExprForTarget(ctx, targetType, right);
            xl6OutputPuts(";\n", ctx->out);
        }
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

    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        const char* cType = targetType != NULL ? xl6CgenTypeName(ctx, targetType) : NULL;
        char cName[512];

        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
        if ( xl6CgenAssignTryStaticXValueCompareStmt(ctx, targetType, cType, cName, right) ) {
            return;
        }
        if ( xl6CgenAssignTryStaticXValueApproxStmt(ctx, targetType, cType, cName, right) ) {
            return;
        }
    }

    xl6CgenIndent(ctx);
    if ( xl6CTextEquals(op, "?=") ) {
        if ( left->kind == XL6_AST_NAME && xl6CgenIsXValueType(xl6CgenNameAssignTargetType(ctx, left->text)) ) {
            int id = ctx->tempId++;
            char cName[512];
            const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);

            xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
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
