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

static int xl6CgenStaticScalarXValueExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteTarget;
    const char* targetName;

    if ( ctx == NULL || targetType == NULL || expr == NULL ) {
        return 0;
    }

    concreteTarget = xl6CgenResolveAliasType(ctx, targetType);
    targetName = concreteTarget != NULL ? concreteTarget->text : NULL;
    if ( targetName == NULL ||
         (!xl6CgenIsIntegerTypeName(targetName) &&
          !xl6CgenIsFloatTypeName(targetName) &&
          !xl6CgenIsBoolTypeName(targetName) &&
          !xl6CTextEquals(targetName, "time") &&
          !xl6CTextEquals(targetName, "point")) ) {
        return 0;
    }

    return xl6CgenExprNeedsXValueScalarBridge(ctx, concreteTarget, expr);
}

static int xl6CgenStaticScalarFromXValueExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* concreteTarget;
    const char* targetName;

    if ( !xl6CgenStaticScalarXValueExpr(ctx, targetType, expr) ) {
        return 0;
    }

    concreteTarget = xl6CgenResolveAliasType(ctx, targetType);
    targetName = concreteTarget != NULL ? concreteTarget->text : NULL;
    if ( xl6CgenIsIntegerTypeName(targetName) ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(targetName), "xvoGetInt", expr);
    } else if ( xl6CgenIsFloatTypeName(targetName) ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(targetName), "xvoGetFloat", expr);
    } else if ( xl6CgenIsBoolTypeName(targetName) ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(targetName), "xvoGetBool", expr);
    } else if ( xl6CTextEquals(targetName, "time") ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(targetName), "xvoGetTime", expr);
    } else if ( xl6CTextEquals(targetName, "point") ) {
        xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(targetName), "xvoGetPoint", expr);
    } else {
        return 0;
    }
    return 1;
}

static int xl6CgenAssignTryStaticXValueScalarStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const char* cName,
    const XL6_AstNode* right)
{
    if ( ctx == NULL || targetType == NULL || cName == NULL ||
         !xl6CgenStaticScalarXValueExpr(ctx, targetType, right) ) {
        return 0;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", cName);
    if ( !xl6CgenStaticScalarFromXValueExpr(ctx, targetType, right) ) {
        xl6CgenError(ctx, right, "xvalue scalar assignment lowering failed");
        xl6CgenDefaultValue(ctx, targetType);
    }
    xl6OutputPuts(";\n", ctx->out);
    return 1;
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

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_assign_%d = 0;\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    {
        char leftName[64];
        char rightName[64];
        snprintf(leftName, sizeof(leftName), "__xl6_bin_left_%d", id);
        snprintf(rightName, sizeof(rightName), "__xl6_bin_right_%d", id);
        xl6CgenXValueLocalInitStmt(ctx, targetType, leftName, leftExpr);
        xl6CgenXValueLocalInitStmt(ctx, targetType, rightName, rightExpr);
    }
    xl6CgenIndent(ctx);
    {
        int mode = xl6CgenXValueBinaryMode(right->text);
        if ( mode == 3 || mode == 4 || mode == 8 || mode == 9 ) {
            xl6OutputPrintf(ctx->out,
                            "__xl6_assign_%d = xl6_xrt_basic_binary_checked(__xl6_bin_left_%d, __xl6_bin_right_%d, %d, %d, %d);\n",
                            id, id, id, mode, right->line, right->column);
        } else {
            xl6OutputPrintf(ctx->out,
                            "__xl6_assign_%d = xl6_xrt_basic_binary(__xl6_bin_left_%d, __xl6_bin_right_%d, %d);\n",
                            id, id, id, mode);
        }
    }
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

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_assign_%d = 0;\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("if (", ctx->out);
    xl6CgenConditionExpr(ctx, cond);
    xl6OutputPuts(") {\n", ctx->out);
    ctx->indent += 1;
    {
        char assignName[64];
        snprintf(assignName, sizeof(assignName), "__xl6_assign_%d", id);
        xl6CgenXValueAssignStmt(ctx, targetType, assignName, thenExpr);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    {
        char assignName[64];
        snprintf(assignName, sizeof(assignName), "__xl6_assign_%d", id);
        xl6CgenXValueAssignStmt(ctx, targetType, assignName, elseExpr);
    }
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
         xl6CgenClassProtocolBinaryAvailable(ctx, right) ||
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
        if ( xl6CgenBinaryCompareHasClassHandle(ctx, right) ) {
            xl6OutputPrintf(ctx->out,
                            "int __xl6_cmp_result_%d = "
                            "(((__xl6_cmp_left_%d == 0 || xvoIsNull(__xl6_cmp_left_%d)) && "
                            "(__xl6_cmp_right_%d == 0 || xvoIsNull(__xl6_cmp_right_%d))) || "
                            "(__xl6_cmp_left_%d != 0 && __xl6_cmp_right_%d != 0 && "
                            "!xvoIsNull(__xl6_cmp_left_%d) && !xvoIsNull(__xl6_cmp_right_%d) && "
                            "__xl6_cmp_left_%d == __xl6_cmp_right_%d));\n",
                            id, id, id, id, id, id, id, id, id, id, id);
        } else {
            xl6OutputPrintf(ctx->out,
                            "int __xl6_cmp_result_%d = xl6_xrt_basic_equal(__xl6_cmp_left_%d, __xl6_cmp_right_%d);\n",
                            id,
                            id,
                            id);
        }
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
    char slotName[64];

    if ( ctx == NULL || left == NULL || right == NULL || !xl6CgenIsXValueType(targetType) ) {
        return 0;
    }

    id = ctx->tempId++;
    snprintf(slotName, sizeof(slotName), "__xl6_xvalue_field_slot_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* %s = &(", slotName);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts(");\n", ctx->out);
    if ( !xl6CgenAssignTryXValueConditionalStmt(ctx, targetType, right, id) &&
         !xl6CgenAssignTryXValueBinaryStmt(ctx, targetType, right, id) ) {
        char assignName[64];
        snprintf(assignName, sizeof(assignName), "__xl6_assign_%d", id);
        xl6CgenXValueLocalInitStmt(ctx, targetType, assignName, right);
    }

    /* class 拥有的动态值和嵌套 class 句柄必须在写入时完成跨线程发布。 */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "__xl6_assign_%d = xl6_xrt_move_to_shared(__xl6_assign_%d);\n",
                    id,
                    id);

    /* var 字段拥有 xvalue 引用，替换前必须释放旧值。 */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (*%s != 0) {\n", slotName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(*%s);\n", slotName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "*%s = __xl6_assign_%d;\n", slotName, id);
    return 1;
}
