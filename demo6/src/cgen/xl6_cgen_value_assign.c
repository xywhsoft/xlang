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
    const XL6_AstNode* externalVar;
    unsigned int flags;
    char cName[512];

    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    if ( xl6CgenImplicitSelfFieldPath(ctx, name, buffer, capacity) ) {
        return;
    }
    flags = xl6CgenFindTrackedVarFlags(ctx, name);
    if ( (flags & (XL6_CGEN_VAR_BYREF | XL6_CGEN_VAR_OUT)) != 0 ) {
        xl6CgenCIdentText(cName, sizeof(cName), name);
        snprintf(buffer, capacity, "(*%s)", cName);
        return;
    }
    if ( xl6CgenFindTrackedVarType(ctx, name) == NULL ) {
        externalVar = xl6CgenFindExternalVar(ctx, name);
        if ( externalVar != NULL ) {
            xl6CgenExternalVarStorageNameText(buffer, capacity, externalVar);
            return;
        }
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
            xl6OutputPuts(", (long long)(", ctx->out);
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
    result = xl6CgenClassHandleTargetType(ctx, receiverType) == NULL &&
             xl6CgenIsXValueType(receiverType);
    xl6AstFree(receiverType);
    return result;
}

static int xl6CgenDynamicMemberAssignStmt(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    int id;
    int receiverBorrow;
    int receiverNeedsMutable;
    char receiverName[64];
    char tempName[64];

    if ( left == NULL || left->kind != XL6_AST_MEMBER || left->text == NULL || left->childCount == 0 ) {
        return 0;
    }

    receiver = left->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    if ( xl6CgenClassHandleTargetType(ctx, receiverType) != NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }
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
    receiverNeedsMutable = receiver != NULL &&
        (receiver->kind == XL6_AST_MEMBER || receiver->kind == XL6_AST_SAFE_MEMBER ||
         receiver->kind == XL6_AST_INDEX || receiver->kind == XL6_AST_SAFE_INDEX);
    receiverBorrow = receiverNeedsMutable || xl6CgenExprIsXValueBorrow(ctx, receiver);
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
    if ( receiverNeedsMutable ) {
        xl6CgenXValueWriteReceiverExpr(ctx, receiver);
    } else if ( receiverBorrow ) {
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
#include "xl6_cgen_value_assign_xvalue.c"

/*
    带资源的 class 字段采用真正的值语义：先完整取得右值所有权，再销毁旧值。
    直接生成 C 结构体赋值会让左右两侧共享字符串、容器和闭包，最终双重释放。
*/
static int xl6CgenClassMemberAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const XL6_AstNode* right,
    const XL6_AstNode* fieldType)
{
    XL6_AstNode* classNode;
    XL6_AstNode* ctorClass;
    int id;
    char slotName[64];
    char tempName[64];
    char slotPath[96];

    classNode = xl6CgenClassNodeForType(ctx, fieldType);
    if ( classNode == NULL ||
         xl6CgenFindNativeBackedClass(ctx, classNode->text) != NULL ||
         !xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        return 0;
    }

    id = ctx->tempId++;
    snprintf(slotName, sizeof(slotName), "__xl6_class_field_slot_%d", id);
    snprintf(tempName, sizeof(tempName), "__xl6_class_field_value_%d", id);
    snprintf(slotPath, sizeof(slotPath), "(*%s)", slotName);

    /* 左值地址只计算一次，属性、索引等复杂接收者不会被重复求值。 */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s* %s = &(", xl6CgenTypeName(ctx, fieldType), slotName);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts(");\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, fieldType), tempName);

    ctorClass = NULL;
    if ( xl6CgenCallIsClassCtor(ctx, right, &ctorClass) &&
         ctorClass != NULL && xl6CTextEquals(classNode->text, ctorClass->text) ) {
        if ( !xl6CgenEmitClassCtorInto(ctx, right, tempName) ) {
            xl6CgenError(ctx, right, "class field constructor assignment lowering failed");
        }
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", tempName);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, fieldType, tempName, right);
    }

    xl6CgenReleaseClassFunctionFieldsForType(ctx, fieldType, slotPath);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "*%s = %s;\n", slotName, tempName);
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
        if ( !xl6CTextEquals(op, "=") && xl6CgenClassPropertyCompoundAssignStmt(ctx, left, op, right) ) {
            return;
        }
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
    if ( !xl6CTextEquals(op, "=") && left->kind == XL6_AST_MEMBER &&
         xl6CgenClassPropertyCompoundAssignStmt(ctx, left, op, right) ) {
        return;
    }
    if ( xl6CgenTypedContainerCompoundAssignStmt(ctx, left, op, right) ) {
        return;
    }
    if ( !xl6CTextEquals(op, "=") && xl6CgenClassProtocolCompoundAssignStmt(ctx, left, op, right) ) {
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_MEMBER ) {
        XL6_AstNode* ctorNativeClass;
        XL6_AstNode* fieldType;
        XL6_AstNode* fieldNativeClass;

        ctorNativeClass = NULL;
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
        if ( xl6CgenIsStringTypeNode(ctx, fieldType) ) {
            int id = ctx->tempId++;
            char slotName[64];
            char valueName[64];

            snprintf(slotName, sizeof(slotName), "__xl6_string_field_slot_%d", id);
            snprintf(valueName, sizeof(valueName), "__xl6_string_field_value_%d", id);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "const char** %s = &(", slotName);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(");\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "char* %s = xrtCopyStr(", valueName);
            xl6CgenStringSourceExpr(ctx, fieldType, right);
            xl6OutputPuts(", 0u);\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xrtFree((void*)*%s);\n", slotName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "*%s = %s;\n", slotName, valueName);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenIsSpecialCollectionType(ctx, fieldType) ) {
            int id = ctx->tempId++;
            char slotName[64];
            char valueName[64];
            char slotExpr[72];

            snprintf(slotName, sizeof(slotName), "__xl6_special_field_slot_%d", id);
            snprintf(valueName, sizeof(valueName), "__xl6_special_field_value_%d", id);
            snprintf(slotExpr, sizeof(slotExpr), "*%s", slotName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s* %s = &(", xl6CgenTypeName(ctx, fieldType), slotName);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(");\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, fieldType), valueName);
            if ( xl6CgenSpecialCollectionExprIsOwned(ctx, right) ) {
                xl6CgenExpr(ctx, right);
            } else {
                xl6CgenSpecialCollectionRetainExpr(ctx, fieldType, right);
            }
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6CgenSpecialCollectionReleaseCExpr(ctx, fieldType, slotExpr);
            xl6OutputPuts(";\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "*%s = %s;\n", slotName, valueName);
            xl6AstFree(fieldType);
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
        if ( xl6CgenCallIsNativeBackedCtor(ctx, right, &ctorNativeClass) ) {
            fieldNativeClass = fieldType != NULL ? xl6CgenFindNativeBackedClass(ctx, fieldType->text) : NULL;
            if ( fieldNativeClass == NULL ) {
                xl6AstFree(fieldType);
                xl6CgenError(ctx, left, "native-backed class constructor can only assign to same-type native-backed class field");
                return;
            }
            if ( !xl6CTextEquals(fieldNativeClass->text, ctorNativeClass->text) ) {
                xl6CgenError(ctx, right, "native-backed class constructor type does not match target field type");
                xl6AstFree(fieldType);
                return;
            }
            xl6CgenIndent(ctx);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(".handle = ", ctx->out);
            xl6CgenNativeBackedCtorValue(ctx, ctorNativeClass, right);
            xl6OutputPuts(";\n", ctx->out);
            xl6AstFree(fieldType);
            return;
        }
        if ( xl6CgenClassMemberAssignStmt(ctx, left, right, fieldType) ) {
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
         xl6CgenIsSpecialCollectionType(ctx, xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        int id = ctx->tempId++;
        char targetName[512];
        char tempName[64];

        xl6CgenNameAssignTargetPath(ctx, left->text, targetName, sizeof(targetName));
        snprintf(tempName, sizeof(tempName), "__xl6_special_assign_%d", id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, targetType), tempName);
        if ( xl6CgenSpecialCollectionExprIsOwned(ctx, right) ) {
            xl6CgenExpr(ctx, right);
        } else {
            xl6CgenSpecialCollectionRetainExpr(ctx, targetType, right);
        }
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6CgenSpecialCollectionReleaseCExpr(ctx, targetType, targetName);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = %s;\n", targetName, tempName);
        return;
    }
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsTypedContainerType(ctx, xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        const XL6_AstNode* targetType = xl6CgenNameAssignTargetType(ctx, left->text);
        const XL6_AstNode* literal = xl6CgenTypedContainerInitLiteral(ctx, targetType, right);
        XL6_AstNode* targetCanonical = xl6CgenCloneCanonicalTypeNode(ctx, targetType);
        XL6_AstNode* rightType = literal == NULL ? xl6CgenCloneExprTypeForInference(ctx, right) : NULL;
        int moveReturn = literal == NULL && xl6CgenTypedContainerCanMoveFromExpr(ctx, targetType, right);
        int importDynamic = literal == NULL && xl6CgenTypedContainerCanImportDynamicExpr(ctx, targetType, right);
        unsigned int targetFlags = xl6CgenFindTrackedVarFlags(ctx, left->text);
        int targetBorrowed = (targetFlags & XL6_CGEN_VAR_BORROWED) != 0 &&
                             (targetFlags & (XL6_CGEN_VAR_BYREF | XL6_CGEN_VAR_OUT)) == 0;
        int targetShared = (targetFlags & XL6_CGEN_VAR_MODULE_GLOBAL) != 0;
        int id = ctx->tempId++;
        char cName[512];
        char tempName[64];
        char sourceName[64];
        char memberTempName[64];
        XL6_AstNode* memberFunctionType;

        if ( literal == NULL && !importDynamic &&
             (targetCanonical == NULL || rightType == NULL || !xl6CgenTypeNodeEqual(targetCanonical, rightType)) ) {
            xl6CgenError(ctx, right, "typed container assignment requires the same container type or literal");
            xl6AstFree(targetCanonical);
            xl6AstFree(rightType);
            return;
        }
        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
        snprintf(tempName, sizeof(tempName), "__xl6_typed_container_assign_%d", id);
        snprintf(sourceName, sizeof(sourceName), "__xl6_typed_container_source_%d", id);
        memberTempName[0] = '\0';
        memberFunctionType = NULL;
        if ( moveReturn ) {
            memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, right);
            if ( memberFunctionType != NULL ) {
                snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", id);
                xl6CgenEmitMemberFunctionStatementTemp(ctx, right, memberTempName);
            }
        }
        if ( moveReturn && targetShared ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, targetType), sourceName);
            if ( memberFunctionType != NULL ) {
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, right, memberFunctionType, memberTempName);
            } else {
                xl6CgenExpr(ctx, right);
            }
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypedContainerCType(ctx, targetType), tempName);
        if ( literal != NULL ) {
            xl6CgenTypedContainerCreateExprMode(
                ctx,
                targetType,
                targetShared ? "XRT_OBJMODE_SHARED" : "XRT_OBJMODE_LOCAL");
        } else if ( moveReturn && targetShared ) {
            xl6CgenTypedContainerCopyCExprMode(ctx, targetType, sourceName, "XRT_OBJMODE_SHARED");
        } else if ( moveReturn ) {
            if ( memberFunctionType != NULL ) {
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, right, memberFunctionType, memberTempName);
            } else {
                xl6CgenExpr(ctx, right);
            }
        } else if ( importDynamic ) {
            xl6CgenTypedContainerImportDynamicExprMode(
                ctx,
                targetType,
                right,
                targetShared ? "XRT_OBJMODE_SHARED" : "XRT_OBJMODE_LOCAL");
        } else {
            if ( targetShared && !xl6CgenTypedContainerExprIsSharedStorage(ctx, right) ) {
                xl6CgenTypedContainerCopyExprMode(ctx, targetType, right, "XRT_OBJMODE_SHARED");
            } else {
            xl6CgenTypedContainerRetainExpr(ctx, targetType, right);
            }
        }
        xl6OutputPuts(";\n", ctx->out);
        if ( importDynamic ) {
            xl6CgenTypedContainerImportCheckStmt(ctx, tempName, right);
        }
        if ( moveReturn && targetShared ) {
            xl6CgenIndent(ctx);
            xl6CgenTypedContainerDestroyValue(ctx, targetType, sourceName);
        }
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

        if ( classNode != NULL &&
             !xl6CgenIsXValueType(targetType) &&
             (upcastAssign || xl6CgenClassNeedsDestroy(ctx, classNode)) ) {
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
            xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, targetType), tempName);
            if ( upcastAssign ) {
                if ( !xl6CgenClassUpcastStmt(ctx, targetType, right, tempName) ) {
                    xl6CgenError(ctx, right, "class assignment upcast lowering failed");
                }
            } else if ( memberFunctionType != NULL ) {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = ", tempName);
                xl6CgenMemberFunctionStatementInvokeExpr(ctx, right, memberFunctionType, memberTempName);
                xl6OutputPuts(";\n", ctx->out);
                xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, targetType, tempName, right);
            } else {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "%s = ", tempName);
                xl6CgenExpr(ctx, right);
                xl6OutputPuts(";\n", ctx->out);
                xl6CgenRetainClassFunctionFieldsForBorrowedExpr(ctx, targetType, tempName, right);
            }
            xl6AstFree(memberFunctionType);
            xl6CgenReleaseClassFunctionFieldsForType(ctx, targetType, cName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);
            if ( (xl6CgenFindTrackedVarFlags(ctx, left->text) & XL6_CGEN_VAR_MODULE_GLOBAL) != 0 ) {
                xl6CgenClassValuePublishShared(ctx, targetType, cName);
            }
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
        int targetShared = (xl6CgenFindTrackedVarFlags(ctx, left->text) & XL6_CGEN_VAR_MODULE_GLOBAL) != 0;

        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));

        if ( !xl6CgenAssignTryXValueConditionalStmt(ctx, targetType, right, id) &&
             !xl6CgenAssignTryXValueBinaryStmt(ctx, targetType, right, id) ) {
            char assignName[64];
            snprintf(assignName, sizeof(assignName), "__xl6_assign_%d", id);
            xl6CgenXValueLocalInitStmt(ctx, targetType, assignName, right);
        }
        if ( targetShared ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out,
                            "__xl6_assign_%d = xl6_xrt_move_to_shared(__xl6_assign_%d);\n",
                            id,
                            id);
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
    if ( xl6CTextEquals(op, "=") && left->kind == XL6_AST_NAME &&
         xl6CgenIsStringTypeNode(ctx, xl6CgenNameAssignTargetType(ctx, left->text)) ) {
        int id = ctx->tempId++;
        char cName[512];

        xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "char* __xl6_string_assign_%d = xrtCopyStr(", id);
        xl6CgenStringSourceExpr(ctx, xl6CgenNameAssignTargetType(ctx, left->text), right);
        xl6OutputPuts(", 0u);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                        "if (__xl6_string_assign_%d == 0) { xl6_runtime_fatal(3); }\n",
                        id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtFree((void*)%s);\n", cName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = __xl6_string_assign_%d;\n", cName, id);
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
            int targetShared = (xl6CgenFindTrackedVarFlags(ctx, left->text) & XL6_CGEN_VAR_MODULE_GLOBAL) != 0;

            xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
            xl6OutputPrintf(ctx->out, "if (xvoIsNull(%s)) {\n", cName);
            ctx->indent += 1;
            {
                char assignName[64];
                snprintf(assignName, sizeof(assignName), "__xl6_assign_%d", id);
                xl6CgenXValueLocalInitStmt(ctx, targetType, assignName, right);
            }
            if ( targetShared ) {
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out,
                                "__xl6_assign_%d = xl6_xrt_move_to_shared(__xl6_assign_%d);\n",
                                id,
                                id);
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
        int id = ctx->tempId++;

        xl6OutputPrintf(ctx->out, "const char** __xl6_string_slot_%d = &(", id);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                        "char* __xl6_string_value_%d = xrtCopyStr(xl6_xrt_str_concat(*__xl6_string_slot_%d, ",
                        id,
                        id);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts("), 0u);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_string_value_%d == 0) { xl6_runtime_fatal(3); }\n", id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtFree((void*)*__xl6_string_slot_%d);\n", id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "*__xl6_string_slot_%d = __xl6_string_value_%d;\n", id, id);
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
