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
    xl6CgenValueStorageNameText(ctx, buffer, capacity, name);
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
        xl6OutputPrintf(ctx->out, "xvalue* %s = xl6_xrt_typed_box_or_null(", currentName);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        if ( xl6CTextEquals(receiverKind, "array") ) {
            xl6OutputPuts("xl6_xrt_typed_array_get_at(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CTextEquals(receiverKind, "list") ) {
            xl6OutputPuts("xl6TypedListGet(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPuts("))", ctx->out);
        } else {
            xl6CgenTypedDictGetRawExpr(ctx, receiver, index);
        }
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0 || xl6_xrt_value_kind(%s) == XL6_XVO_DT_NULL) {\n", currentName, currentName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRelease(%s); %s = 0; }\n", currentName, currentName, currentName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue* %s = ", tempName);
        xl6CgenXValueFromExpr(ctx, right);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndexAssignTemp(ctx, left, tempName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtValueRelease(%s);\n", currentName);
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
        xl6OutputPrintf(ctx->out, "xvalue* %s = xl6_xrt_value_text_create((void*)xl6_xrt_str_concat(%s, %s), 0, false);\n",
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
        xl6OutputPrintf(ctx->out, "xvalue* %s = xrtValueInt(xl6_int_pow_i64(%s, %s));\n", tempName, currentName, rightName);
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
        xl6OutputPrintf(ctx->out, "xvalue* %s = xrtValueFloat(%s %s %s);\n", tempName, currentName, binaryOp, rightName);
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
        xl6OutputPrintf(ctx->out, "xvalue* %s = xrtValueInt(%s %s %s);\n", tempName, currentName, binaryOp, rightName);
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
        xl6_xrt_index_get_i64/xl6_xrt_value_object_get 返回容器内借用值。
        current 只用于本语句内读取旧值，不能 xrtValueRelease，否则会误减容器元素引用。
    */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* %s = ", currentName);
    xl6CgenIndexExpr(ctx, left);
    xl6OutputPuts(";\n", ctx->out);

    if ( xl6CTextEquals(op, "?=") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (xl6_xrt_value_is_null(%s)) {\n", currentName);
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
        xl6OutputPrintf(ctx->out, "xvalue* %s = xrtValueInt(xl6_int_pow_i64(xl6_xrt_value_int(%s), %s));\n",
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
        xl6OutputPrintf(ctx->out, "int %s = xl6_xrt_value_kind(%s);\n", typeName, currentName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out,
                "xvalue* %s = xrtValueFloat(xl6_xrt_value_to_double(%s, %s) %s %s);\n",
                tempName, currentName, typeName, binaryOp, rightName);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = (long long)(", rightName);
        xl6CgenXValueAsI64Expr(ctx, right);
        xl6OutputPuts(");\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "int %s = xl6_xrt_value_kind(%s);\n", typeName, currentName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue* %s = 0;\n", tempName);
        if ( xl6CTextEquals(op, "%=") ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = xrtValueInt(xl6_xrt_value_int(%s) %% %s);\n",
                    tempName, currentName, rightName);
        } else {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s == XL6_XVO_DT_FLOAT) {\n", typeName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out,
                    "%s = xrtValueFloat(xl6_xrt_value_to_double(%s, %s) %s (double)%s);\n",
                    tempName, currentName, typeName, binaryOp, rightName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("} else {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = xrtValueInt(xl6_xrt_value_int(%s) %s %s);\n",
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
    receiverType = xl6CgenCloneExprStorageType(ctx, receiver);
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
    /*
     * 动态成员写入取决于接收者的真实 C 存储，而不是流分析后的读取类型。
     * 例如 `var boxed = objectValue; boxed.extra = 1` 中 boxed 仍由 xvalue* 承载。
     */
    receiverType = xl6CgenCloneExprStorageType(ctx, receiver);
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
        xl6OutputPrintf(ctx->out, "xvalue* %s = ", receiverName);
        /*
            嵌套动态字段赋值需要先把 receiver 固化为 owned xvalue*。
            例如 nested.inner.name = 1：inner 是从 nested 取出的字典对象，
            写入 name 后立即释放这个临时引用，底层对象仍由 nested 持有。
        */
        xl6CgenXValueFromExpr(ctx, receiver);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* %s = ", tempName);
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
    xl6OutputPrintf(ctx->out, "if ( %s != 0 ) { xrtValueRelease(%s); }\n", tempName, tempName);
    if ( !receiverBorrow ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if ( %s != 0 ) { xrtValueRelease(%s); }\n", receiverName, receiverName);
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

/*
 * nullable 静态容器和普通静态容器共享句柄 ABI，但不共享默认值和赋值语义：
 * T 默认创建空容器，T? 默认是 null。这里必须在普通容器赋值之前完成 lowering。
 */
static int xl6CgenNullableTypedContainerAssignStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* left,
    const XL6_AstNode* right,
    const XL6_AstNode* targetType)
{
    const XL6_AstNode* innerType;
    const XL6_AstNode* literal;
    XL6_AstNode* targetCanonical;
    XL6_AstNode* rightType;
    const XL6_AstNode* rightConcrete;
    XL6_AstNode* rightCanonical;
    XL6_HirOwnership ownership;
    unsigned int targetFlags;
    int targetBorrowed;
    int id;
    char cName[512];
    char tempName[64];
    char sourceName[64];

    if ( ctx == NULL || left == NULL || left->kind != XL6_AST_NAME ||
         !xl6CgenIsNullableType(targetType) ) {
        return 0;
    }
    innerType = xl6CgenNullableInnerType(targetType);
    if ( !xl6CgenIsTypedContainerType(ctx, innerType) ) {
        return 0;
    }

    literal = xl6CgenTypedContainerInitLiteral(ctx, innerType, right);
    targetCanonical = xl6CgenCloneCanonicalTypeNode(ctx, innerType);
    rightType = right != NULL && right->kind != XL6_AST_NULL && literal == NULL
        ? xl6CgenCloneExprTypeForInference(ctx, right)
        : NULL;
    rightConcrete = xl6CgenIsNullableType(rightType)
        ? xl6CgenNullableInnerType(rightType)
        : rightType;
    rightCanonical = rightConcrete != NULL
        ? xl6CgenCloneCanonicalTypeNode(ctx, rightConcrete)
        : NULL;

    if ( right != NULL && right->kind != XL6_AST_NULL && literal == NULL &&
         (targetCanonical == NULL || rightCanonical == NULL ||
          !xl6CgenTypeNodeEqual(targetCanonical, rightCanonical)) ) {
        xl6CgenError(
            ctx,
            right,
            "nullable typed container assignment requires null, the same container type, or a literal");
        xl6AstFree(targetCanonical);
        xl6AstFree(rightType);
        xl6AstFree(rightCanonical);
        return 1;
    }

    ownership = xl6CgenHirOwnership(ctx, right);
    targetFlags = xl6CgenFindTrackedVarFlags(ctx, left->text);
    targetBorrowed = (targetFlags & XL6_CGEN_VAR_BORROWED) != 0 &&
                     (targetFlags & (XL6_CGEN_VAR_BYREF | XL6_CGEN_VAR_OUT)) == 0;
    id = ctx->tempId++;
    xl6CgenNameAssignTargetPath(ctx, left->text, cName, sizeof(cName));
    snprintf(tempName, sizeof(tempName), "__xl6_nullable_container_assign_%d", id);
    snprintf(sourceName, sizeof(sourceName), "__xl6_nullable_container_source_%d", id);

    if ( right == NULL || right->kind == XL6_AST_NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "%s %s = 0;\n",
            xl6CgenTypedContainerCType(ctx, innerType),
            tempName);
    } else if ( literal != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "%s %s = ",
            xl6CgenTypedContainerCType(ctx, innerType),
            tempName);
        xl6CgenTypedContainerCreateExpr(ctx, innerType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenTypedContainerFillFromLiteral(ctx, tempName, innerType, right);
    } else {
        /*
         * 先求值到 source，避免 nullable 表达式在判空和复制时执行两次。
         * MOVE 可直接进入局部目标；模块全局必须转成 shared 对象。
         */
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "%s %s = ",
            xl6CgenTypedContainerCType(ctx, innerType),
            sourceName);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(";\n", ctx->out);

        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "%s %s = %s != 0 ? ",
            xl6CgenTypedContainerCType(ctx, innerType),
            tempName,
            sourceName);
        if ( ownership == XL6_HIR_OWNERSHIP_MOVE ) {
            xl6OutputPuts(sourceName, ctx->out);
        } else {
            xl6CgenTypedContainerRetainCExpr(ctx, innerType, sourceName);
        }
        xl6OutputPuts(" : 0;\n", ctx->out);
    }

    if ( targetBorrowed ) {
        char ownerName[512];

        xl6CgenTypedParamOwnerName(ownerName, sizeof(ownerName), left->text);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s) {\n", ownerName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenTypedContainerDestroyValue(ctx, innerType, cName);
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
        xl6CgenTypedContainerDestroyValue(ctx, innerType, cName);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = %s;\n", cName, tempName);

    xl6AstFree(targetCanonical);
    xl6AstFree(rightType);
    xl6AstFree(rightCanonical);
    return 1;
}

