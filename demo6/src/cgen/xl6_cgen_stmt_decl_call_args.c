static int xl6CgenDeclCallArgStaticScalarType(const XL6_AstNode* typeNode)
{
    const char* typeName;

    typeName = typeNode != NULL ? typeNode->text : NULL;
    return xl6CgenIsIntegerTypeName(typeName) ||
           xl6CgenIsBoolTypeName(typeName) ||
           xl6CgenIsFloatTypeName(typeName) ||
           xl6CTextEquals(typeName, "string") ||
           xl6CTextEquals(typeName, "time") ||
           xl6CTextEquals(typeName, "point");
}

static int xl6CgenDeclCallArgStaticBoolLikeType(const XL6_AstNode* typeNode)
{
    const char* typeName;

    typeName = typeNode != NULL ? typeNode->text : NULL;
    return xl6CgenIsBoolTypeName(typeName) || xl6CgenIsIntegerTypeName(typeName);
}

static void xl6CgenDeclCallArgTempDefaultValue(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    if ( xl6CgenIsXValueType(typeNode) || xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenDefaultValue(ctx, typeNode);
}

static int xl6CgenDeclCallArgTypeConvertName(XL6_CgenContext* ctx, const XL6_AstNode* expr, const char** outName)
{
    const XL6_AstNode* callee;

    if ( outName != NULL ) {
        *outName = NULL;
    }
    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ||
         !xl6CgenIsBuiltinConvertTypeName(callee->text) ||
         !xl6CgenIsKnownTypeName(ctx, callee->text) ) {
        return 0;
    }

    if ( outName != NULL ) {
        *outName = callee->text;
    }
    return 1;
}

static int xl6CgenDeclExprIsStringType(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* exprType;
    int result;

    exprType = xl6CgenCloneExprTypeForInference(ctx, expr);
    result = exprType != NULL && xl6CTextEquals(exprType->text, "string");
    xl6AstFree(exprType);
    return result;
}

static int xl6CgenDeclCanEmitXValueCallValueTemp(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static int xl6CgenDeclCanEmitCallArgTemp(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr);
static int xl6CgenDeclEmitCallArgTemp(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* cType, const char* cName, const XL6_AstNode* expr);
static int xl6CgenDeclCallExprArgsNeedCallArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* expr);
static int xl6CgenDeclEmitXValueCallExprWithArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr, const char* valueName);
static int xl6CgenDeclEmitScalarCallExprWithArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* targetType, const XL6_AstNode* expr, const char* valueName);

static int xl6CgenDeclStringConvertNeedsCallValueTemp(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* sourceExpr;
    const char* convertName;

    if ( !xl6CgenDeclCallArgTypeConvertName(ctx, expr, &convertName) ||
         !xl6CTextEquals(convertName, "string") ) {
        return 0;
    }

    sourceExpr = xl6CgenTypeConvertArg(expr);
    return xl6CgenDeclCanEmitXValueCallValueTemp(ctx, sourceExpr);
}

static int xl6CgenDeclCanEmitXValueCallValueTemp(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr == NULL ) {
        return 0;
    }
    if ( xl6CgenDeclIsNullCoalesceExpr(expr) ) {
        XL6_AstNode* leftType;
        int ok;

        leftType = xl6CgenCloneExprTypeForInference(ctx, expr->children[0]);
        ok = xl6CgenIsXValueType(leftType);
        xl6AstFree(leftType);
        return ok;
    }
    if ( expr->kind == XL6_AST_BINARY && xl6CgenBinaryArithmeticIsXValue(ctx, expr) ) {
        return 1;
    }
    if ( expr->kind == XL6_AST_CONDITIONAL && xl6CgenConditionalIsXValue(ctx, expr) ) {
        return 1;
    }
    if ( expr->kind == XL6_AST_CALL && xl6CgenDeclCallExprArgsNeedCallArgTemps(ctx, expr) ) {
        return 1;
    }
    return 0;
}

static int xl6CgenDeclEmitXValueCallValueTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* targetType,
    const XL6_AstNode* expr,
    const char* valueName)
{
    int id;

    if ( ctx == NULL || targetType == NULL || expr == NULL || valueName == NULL ||
         !xl6CgenDeclCanEmitXValueCallValueTemp(ctx, expr) ) {
        return 0;
    }

    if ( expr->kind == XL6_AST_CALL && xl6CgenDeclCallExprArgsNeedCallArgTemps(ctx, expr) ) {
        return xl6CgenDeclEmitXValueCallExprWithArgTemps(ctx, targetType, expr, valueName);
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;

    if ( xl6CgenDeclIsNullCoalesceExpr(expr) ) {
        const XL6_AstNode* leftExpr = expr->childCount > 0 ? expr->children[0] : NULL;
        const XL6_AstNode* rightExpr = expr->childCount > 1 ? expr->children[1] : NULL;

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue __xl6_nc_%d = ", id);
        xl6CgenXValueFromExprForTarget(ctx, targetType, leftExpr);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (xvoIsNull(__xl6_nc_%d)) {\n", id);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_nc_%d != 0) {\n", id);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_nc_%d);\n", id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", valueName);
        xl6CgenXValueFromExprForTarget(ctx, targetType, rightExpr);
        xl6OutputPuts(";\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = __xl6_nc_%d;\n", valueName, id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    } else if ( expr->kind == XL6_AST_BINARY ) {
        const XL6_AstNode* leftExpr = expr->childCount > 0 ? expr->children[0] : NULL;
        const XL6_AstNode* rightExpr = expr->childCount > 1 ? expr->children[1] : NULL;

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
                        "%s = xl6_xrt_basic_binary(__xl6_bin_left_%d, __xl6_bin_right_%d, %d);\n",
                        valueName,
                        id,
                        id,
                        xl6CgenXValueBinaryMode(expr->text));
        xl6CgenAssignEmitXValueTempCleanup(ctx, "bin", id);
    } else {
        const XL6_AstNode* cond = expr->childCount > 0 ? expr->children[0] : NULL;
        const XL6_AstNode* thenExpr = expr->childCount > 1 ? expr->children[1] : NULL;
        const XL6_AstNode* elseExpr = expr->childCount > 2 ? expr->children[2] : NULL;

        xl6CgenIndent(ctx);
        xl6OutputPuts("if (", ctx->out);
        xl6CgenConditionExpr(ctx, cond);
        xl6OutputPuts(") {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", valueName);
        xl6CgenXValueFromExprForTarget(ctx, targetType, thenExpr);
        xl6OutputPuts(";\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", valueName);
        xl6CgenXValueFromExprForTarget(ctx, targetType, elseExpr);
        xl6OutputPuts(";\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }

    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclEmitScalarAssignFromXValueTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const char* valueName)
{
    const char* typeName;
    const char* helper;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL || valueName == NULL ) {
        return 0;
    }

    typeName = typeNode->text;
    helper = NULL;
    if ( xl6CgenIsIntegerTypeName(typeName) ) {
        helper = "xl6_xrt_value_to_i64_release";
    } else if ( xl6CgenIsFloatTypeName(typeName) ) {
        helper = "xl6_xrt_value_to_f64_release";
    } else if ( xl6CgenIsBoolTypeName(typeName) ) {
        helper = "xl6_xrt_value_to_bool_release";
    } else if ( xl6CTextEquals(typeName, "string") ) {
        helper = "xl6_xrt_value_to_string_release";
    } else if ( xl6CTextEquals(typeName, "time") ) {
        helper = "xl6_xrt_value_to_time_release";
    } else if ( xl6CTextEquals(typeName, "point") ) {
        helper = "xl6_xrt_value_to_point_release";
    }
    if ( helper == NULL ) {
        return 0;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = (%s)%s(%s);\n", cName, cType, helper, valueName);
    return 1;
}

static int xl6CgenDeclEmitStringExprTemp(XL6_CgenContext* ctx, const XL6_AstNode* expr, const char* cName)
{
    const XL6_AstNode* sourceExpr;
    const XL6_AstNode* targetType;
    char valueName[64];
    int id;

    if ( ctx == NULL || expr == NULL || cName == NULL || !xl6CgenDeclExprIsStringType(ctx, expr) ) {
        return 0;
    }

    if ( xl6CgenDeclStringConvertNeedsCallValueTemp(ctx, expr) ) {
        sourceExpr = xl6CgenTypeConvertArg(expr);
        targetType = expr->childCount > 0 ? expr->children[0] : NULL;
        id = ctx->tempId++;
        snprintf(valueName, sizeof(valueName), "__xl6_call_value_%d", id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", valueName);
        if ( !xl6CgenDeclEmitXValueCallValueTemp(ctx, targetType, sourceExpr, valueName) ) {
            return 0;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "const char* %s = (const char*)xl6_xrt_value_to_string_release(%s);\n", cName, valueName);
        return 1;
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "const char* %s = ", cName);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts(";\n", ctx->out);
    return 1;
}

static int xl6CgenDeclStringCompareNeedsCallArgTemp(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* leftExpr;
    const XL6_AstNode* rightExpr;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         !(xl6CTextEquals(expr->text, "==") || xl6CTextEquals(expr->text, "!=") ||
           xl6CTextEquals(expr->text, "<") || xl6CTextEquals(expr->text, "<=") ||
           xl6CTextEquals(expr->text, ">") || xl6CTextEquals(expr->text, ">=")) ) {
        return 0;
    }

    leftExpr = expr->children[0];
    rightExpr = expr->children[1];
    if ( !xl6CgenDeclExprIsStringType(ctx, leftExpr) || !xl6CgenDeclExprIsStringType(ctx, rightExpr) ) {
        return 0;
    }

    return xl6CgenDeclStringConvertNeedsCallValueTemp(ctx, leftExpr) ||
           xl6CgenDeclStringConvertNeedsCallValueTemp(ctx, rightExpr);
}

static int xl6CgenDeclEmitStringCompareCallArgTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* leftExpr;
    const XL6_AstNode* rightExpr;
    char leftName[64];
    char rightName[64];
    int id;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         !xl6CgenDeclCallArgStaticBoolLikeType(typeNode) ||
         !xl6CgenDeclStringCompareNeedsCallArgTemp(ctx, expr) ) {
        return 0;
    }

    leftExpr = expr->children[0];
    rightExpr = expr->children[1];
    id = ctx->tempId++;
    snprintf(leftName, sizeof(leftName), "__xl6_call_str_left_%d", id);
    snprintf(rightName, sizeof(rightName), "__xl6_call_str_right_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    if ( !xl6CgenDeclEmitStringExprTemp(ctx, leftExpr, leftName) ||
         !xl6CgenDeclEmitStringExprTemp(ctx, rightExpr, rightName) ) {
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return 0;
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "int __xl6_call_str_cmp_%d = xrtStrComp(%s, %s, 0, false);\n", id, leftName, rightName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = (%s)(", cName, cType);
    if ( xl6CTextEquals(expr->text, "==") ) {
        xl6OutputPrintf(ctx->out, "__xl6_call_str_cmp_%d == 0", id);
    } else if ( xl6CTextEquals(expr->text, "!=") ) {
        xl6OutputPrintf(ctx->out, "__xl6_call_str_cmp_%d != 0", id);
    } else if ( xl6CTextEquals(expr->text, "<") ) {
        xl6OutputPrintf(ctx->out, "__xl6_call_str_cmp_%d < 0", id);
    } else if ( xl6CTextEquals(expr->text, "<=") ) {
        xl6OutputPrintf(ctx->out, "__xl6_call_str_cmp_%d <= 0", id);
    } else if ( xl6CTextEquals(expr->text, ">") ) {
        xl6OutputPrintf(ctx->out, "__xl6_call_str_cmp_%d > 0", id);
    } else {
        xl6OutputPrintf(ctx->out, "__xl6_call_str_cmp_%d >= 0", id);
    }
    xl6OutputPuts(");\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclStaticNullCoalesceCallArgMatches(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int result;

    if ( ctx == NULL || typeNode == NULL || !xl6CgenDeclIsNullCoalesceExpr(expr) ||
         !xl6CgenDeclCanLowerStaticNullCoalesceType(ctx, typeNode) ) {
        return 0;
    }

    left = expr->children[0];
    right = expr->children[1];
    if ( left == NULL || right == NULL || left->kind == XL6_AST_NULL ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    result = leftType != NULL &&
             rightType != NULL &&
             xl6CgenTypeNodeEqual(leftType, typeNode) &&
             xl6CgenTypeNodeEqual(rightType, typeNode);
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return result;
}

static int xl6CgenDeclTryStaticNullCoalesceCallArgTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         !xl6CgenDeclStaticNullCoalesceCallArgMatches(ctx, typeNode, expr) ) {
        return 0;
    }

    left = expr->children[0];
    right = expr->children[1];
    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s __xl6_nc_%d = ", cType, id);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_nc_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = __xl6_nc_%d;\n", cName, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", cName);
    xl6CgenExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryTypeConvertCallArgTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* sourceExpr;
    const char* convertName;
    char valueName[64];
    int id;

    if ( !xl6CgenDeclCallArgStaticScalarType(typeNode) ||
         !xl6CgenDeclCallArgTypeConvertName(ctx, expr, &convertName) ||
         !xl6CTextEquals(convertName, typeNode != NULL ? typeNode->text : NULL) ) {
        return 0;
    }

    sourceExpr = xl6CgenTypeConvertArg(expr);
    if ( !xl6CgenDeclCanEmitXValueCallValueTemp(ctx, sourceExpr) ) {
        return 0;
    }

    id = ctx->tempId++;
    snprintf(valueName, sizeof(valueName), "__xl6_call_value_%d", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = 0;\n", valueName);
    if ( !xl6CgenDeclEmitXValueCallValueTemp(ctx, typeNode, sourceExpr, valueName) ) {
        return 0;
    }
    return xl6CgenDeclEmitScalarAssignFromXValueTemp(ctx, typeNode, cType, cName, valueName);
}

static int xl6CgenDeclTryLogicalCallArgTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* expr)
{
    const XL6_AstNode* leftExpr;
    const XL6_AstNode* rightExpr;
    int id;
    int isOr;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         expr == NULL || expr->kind != XL6_AST_BINARY || expr->childCount < 2 ||
         !xl6CgenDeclCallArgStaticBoolLikeType(typeNode) ||
         !xl6CgenBinaryLogicalIsXValue(ctx, expr) ) {
        return 0;
    }

    leftExpr = expr->children[0];
    rightExpr = expr->children[1];
    id = ctx->tempId++;
    isOr = xl6CTextEquals(expr->text, "or");

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_logic_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, leftExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "int __xl6_logic_left_truth_%d = xl6_xrt_value_truthy(__xl6_logic_left_%d);\n", id, id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_logic_left_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_logic_left_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = (%s)(%s);\n", cName, cType, isOr ? "1" : "0");
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s__xl6_logic_left_truth_%d) {\n", isOr ? "!" : "", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue __xl6_logic_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, rightExpr);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = (%s)xl6_xrt_value_truthy(__xl6_logic_right_%d);\n", cName, cType, id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_logic_right_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(__xl6_logic_right_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

/*
    临时 class 接收者上的函数字段调用独立成小分片。
    它依赖本文件前面的标量参数判断，后续 call arg temp 入口直接复用。
*/
#include "xl6_cgen_stmt_decl_call_member_temp.c"

static int xl6CgenDeclCanEmitCallArgTemp(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const XL6_AstNode* expr)
{
    const XL6_AstNode* sourceExpr;
    const char* convertName;

    if ( ctx == NULL || typeNode == NULL || expr == NULL ) {
        return 0;
    }
    if ( xl6CgenIsTypedContainerType(ctx, typeNode) &&
         (xl6CgenTypedContainerInitLiteral(ctx, typeNode, expr) != NULL ||
          xl6CgenTypedContainerCanCopyFromExpr(ctx, typeNode, expr) ||
          xl6CgenTypedContainerCanMoveFromExpr(ctx, typeNode, expr)) ) {
        return 1;
    }
    if ( xl6CgenDeclCanEmitMemberFunctionTempReceiverCallTemp(ctx, typeNode, expr) ) {
        return 1;
    }
    if ( xl6CgenIsXValueType(typeNode) && xl6CgenDeclCanEmitXValueCallValueTemp(ctx, expr) ) {
        return 1;
    }
    if ( xl6CgenDeclCallArgStaticBoolLikeType(typeNode) &&
         (xl6CgenBinaryCompareIsXValue(ctx, expr) ||
          xl6CgenDeclStringCompareNeedsCallArgTemp(ctx, expr) ||
          xl6CgenBinaryApproxIsXValue(ctx, expr) ||
          xl6CgenBinaryLogicalIsXValue(ctx, expr) ||
          (expr->kind == XL6_AST_BINARY && xl6CTextEquals(expr->text, "in"))) ) {
        return 1;
    }
    if ( xl6CgenDeclCallArgStaticScalarType(typeNode) &&
         expr->kind == XL6_AST_CALL &&
         xl6CgenDeclCallExprArgsNeedCallArgTemps(ctx, expr) ) {
        return 1;
    }
    if ( xl6CgenDeclStaticNullCoalesceCallArgMatches(ctx, typeNode, expr) ) {
        return 1;
    }

    if ( xl6CgenDeclCallArgStaticScalarType(typeNode) &&
         xl6CgenDeclCallArgTypeConvertName(ctx, expr, &convertName) &&
         xl6CTextEquals(convertName, typeNode->text) ) {
        sourceExpr = xl6CgenTypeConvertArg(expr);
        return xl6CgenDeclCanEmitXValueCallValueTemp(ctx, sourceExpr);
    }

    return 0;
}

static int xl6CgenDeclEmitCallArgTemp(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* expr)
{
    if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        int initByCopy = xl6CgenTypedContainerCanCopyFromExpr(ctx, typeNode, expr);
        int initByMove = xl6CgenTypedContainerCanMoveFromExpr(ctx, typeNode, expr);

        if ( xl6CgenTypedContainerInitLiteral(ctx, typeNode, expr) == NULL && !initByCopy && !initByMove ) {
            return 0;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", cName);
        if ( initByCopy ) {
            xl6CgenTypedContainerCopyExpr(ctx, typeNode, expr);
        } else if ( initByMove ) {
            xl6CgenExpr(ctx, expr);
        } else {
            xl6CgenTypedContainerCreateExpr(ctx, typeNode);
        }
        xl6OutputPuts(";\n", ctx->out);
        if ( !initByCopy && !initByMove ) {
            xl6CgenTypedContainerFillFromLiteral(ctx, cName, typeNode, expr);
        }
        (void)cType;
        return 1;
    }
    if ( xl6CgenDeclTryMemberFunctionTempReceiverCallTemp(ctx, typeNode, cType, cName, expr) ) {
        return 1;
    }
    if ( xl6CgenIsXValueType(typeNode) && xl6CgenDeclEmitXValueCallValueTemp(ctx, typeNode, expr, cName) ) {
        return 1;
    }
    if ( xl6CgenAssignTryStaticXValueCompareStmt(ctx, typeNode, cType, cName, expr) ) {
        return 1;
    }
    if ( xl6CgenAssignTryStaticXValueApproxStmt(ctx, typeNode, cType, cName, expr) ) {
        return 1;
    }
    if ( xl6CgenDeclEmitStringCompareCallArgTemp(ctx, typeNode, cType, cName, expr) ) {
        return 1;
    }
    if ( xl6CgenDeclTryLogicalCallArgTemp(ctx, typeNode, cType, cName, expr) ) {
        return 1;
    }
    if ( xl6CgenDeclCallArgStaticBoolLikeType(typeNode) && xl6CgenTryInExprStmt(ctx, expr, cName) ) {
        return 1;
    }
    if ( xl6CgenDeclCallArgStaticScalarType(typeNode) &&
         expr->kind == XL6_AST_CALL &&
         xl6CgenDeclEmitScalarCallExprWithArgTemps(ctx, typeNode, expr, cName) ) {
        (void)cType;
        return 1;
    }
    if ( xl6CgenDeclTryStaticNullCoalesceCallArgTemp(ctx, typeNode, cType, cName, expr) ) {
        return 1;
    }
    if ( xl6CgenDeclTryTypeConvertCallArgTemp(ctx, typeNode, cType, cName, expr) ) {
        return 1;
    }
    return 0;
}

/*
    callable 调用内部实参也可能需要先落到语句级临时量。
    该分片复用上面的 call-arg lowering，避免 kwargs/var-call 路径回退到 C 扩展。
*/
#include "xl6_cgen_stmt_decl_call_nested_args.c"

static int xl6CgenDeclTaskSpawnTempAppend(
    XL6_CgenContext* ctx,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity,
    const XL6_AstNode* expr,
    XL6_AstNode* typeNode)
{
    return xl6CgenDeclCallArgTempAppend(ctx, temps, count, capacity, expr, typeNode, "__xl6_task_arg_", "task.spawn argument temporaries");
}

static int xl6CgenDeclCollectTaskSpawnArgTempsFromCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity)
{
    const XL6_AstNode* paramTypes;
    const XL6_AstNode* fnExpr;
    XL6_AstNode* functionType;
    size_t argCount;
    size_t actualCount;
    size_t fixedCount;
    int hasArrayVararg;
    size_t i;

    if ( ctx == NULL || call == NULL || call->kind != XL6_AST_CALL ) {
        return 0;
    }

    fnExpr = xl6CgenTaskCallArgExprAt(call, 0);
    functionType = xl6CgenCloneExprTypeForInference(ctx, fnExpr);
    paramTypes = xl6CgenFunctionParamTypes(functionType);
    if ( !xl6CgenIsFunctionType(functionType) || paramTypes == NULL ) {
        xl6AstFree(functionType);
        return 0;
    }

    argCount = call->childCount > 1 && call->children[1] != NULL ? call->children[1]->childCount : 0;
    fixedCount = xl6CgenFunctionTypeFixedParamCount(paramTypes);
    hasArrayVararg = paramTypes->childCount > fixedCount &&
                     xl6CgenFunctionTypeParamIsArrayVararg(paramTypes->children[fixedCount]);
    actualCount = argCount > 0 ? argCount - 1 : 0;

    for ( i = 0; i < actualCount; ++i ) {
        const XL6_AstNode* argExpr = xl6CgenTaskCallArgExprAt(call, i + 1);
        const XL6_AstNode* targetType = i < fixedCount ? paramTypes->children[i] : NULL;
        XL6_AstNode* tempType = NULL;

        if ( argExpr == NULL ) {
            continue;
        }
        if ( targetType != NULL ) {
            tempType = xl6CgenCloneCanonicalTypeNode(ctx, targetType);
        } else if ( hasArrayVararg ) {
            tempType = xl6CgenNewSimpleType("var", argExpr);
        }
        if ( tempType == NULL ) {
            continue;
        }
        if ( !xl6CgenDeclCanEmitCallArgTemp(ctx, tempType, argExpr) ) {
            xl6AstFree(tempType);
            continue;
        }
        if ( !xl6CgenDeclTaskSpawnTempAppend(ctx, temps, count, capacity, argExpr, tempType) ) {
            xl6AstFree(functionType);
            return 1;
        }
    }

    xl6AstFree(functionType);
    return 1;
}

static void xl6CgenDeclCollectTaskSpawnArgTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity)
{
    const char* memberName;
    size_t i;

    if ( ctx == NULL || expr == NULL ) {
        return;
    }

    if ( expr->kind == XL6_AST_CALL &&
         xl6CgenIsTaskNamespaceMemberCall(expr, &memberName) &&
         xl6CTextEquals(memberName, "spawn") ) {
        (void)xl6CgenDeclCollectTaskSpawnArgTempsFromCall(ctx, expr, temps, count, capacity);
    }

    for ( i = 0; i < expr->childCount; ++i ) {
        xl6CgenDeclCollectTaskSpawnArgTemps(ctx, expr->children[i], temps, count, capacity);
    }
}

typedef struct XL6_CgenTaskSpawnArgTempScope {
    XL6_CgenDeclCallArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    int tempCount;
    int tempCapacity;
    int active;
} XL6_CgenTaskSpawnArgTempScope;

static void xl6CgenDeclEmitTaskSpawnArgTemps(XL6_CgenContext* ctx, XL6_CgenDeclCallArgTemp* temps, int capacity)
{
    int i;

    if ( ctx == NULL || temps == NULL || capacity <= 0 ) {
        return;
    }

    for ( i = 0; i < capacity; ++i ) {
        const char* tempCType;

        if ( !temps[i].active ) {
            continue;
        }
        tempCType = xl6CgenValueStorageCType(ctx, temps[i].typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPuts(tempCType, ctx->out);
        xl6OutputPrintf(ctx->out, " %s = ", temps[i].name);
        xl6CgenDeclCallArgTempDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, tempCType, temps[i].name, temps[i].expr) ) {
            xl6CgenError(ctx, temps[i].expr, "task.spawn argument lowering failed");
        }
    }
}

static int xl6CgenTaskSpawnArgTempScopeBegin(
    XL6_CgenContext* ctx,
    const XL6_AstNode* expr,
    XL6_CgenTaskSpawnArgTempScope* scope)
{
    if ( scope == NULL ) {
        return 0;
    }
    memset(scope, 0, sizeof(*scope));
    if ( ctx == NULL || expr == NULL ) {
        return 0;
    }

    xl6CgenDeclCollectTaskSpawnArgTemps(ctx, expr, &scope->temps, &scope->tempCount, &scope->tempCapacity);
    if ( scope->tempCount <= 0 ) {
        xl6CgenDeclFreeCallArgTemps(scope->temps, scope->tempCapacity);
        memset(scope, 0, sizeof(*scope));
        return 0;
    }

    scope->replacements = xl6CgenDeclCreateCallArgReplacements(ctx, scope->temps, scope->tempCapacity, scope->tempCount);
    if ( scope->replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(scope->temps, scope->tempCapacity);
        memset(scope, 0, sizeof(*scope));
        return 0;
    }

    xl6CgenDeclEmitTaskSpawnArgTemps(ctx, scope->temps, scope->tempCapacity);
    scope->prevExprReplacements = ctx->exprReplacements;
    scope->prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = scope->replacements;
    ctx->exprReplacementCount = (size_t)scope->tempCount;
    scope->active = 1;
    return 1;
}

static void xl6CgenTaskSpawnArgTempScopeEnd(XL6_CgenContext* ctx, XL6_CgenTaskSpawnArgTempScope* scope)
{
    if ( ctx == NULL || scope == NULL || !scope->active ) {
        return;
    }

    ctx->exprReplacements = scope->prevExprReplacements;
    ctx->exprReplacementCount = scope->prevExprReplacementCount;
    xrtFree(scope->replacements);
    xl6CgenDeclFreeCallArgTemps(scope->temps, scope->tempCapacity);
    memset(scope, 0, sizeof(*scope));
}

static int xl6CgenDeclTryTaskSpawnArgTempsInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    XL6_CgenDeclCallArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    int tempCount;
    int tempCapacity;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL || initExpr == NULL ) {
        return 0;
    }

    temps = NULL;
    replacements = NULL;
    tempCount = 0;
    tempCapacity = 0;
    xl6CgenDeclCollectTaskSpawnArgTemps(ctx, initExpr, &temps, &tempCount, &tempCapacity);
    if ( tempCount <= 0 ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCapacity);
        return 0;
    }

    replacements = xl6CgenDeclCreateCallArgReplacements(ctx, temps, tempCapacity, tempCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, tempCapacity);
        return 0;
    }

    xl6OutputPuts(cType, ctx->out);
    xl6OutputPrintf(ctx->out, " %s = ", cName);
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPuts("0", ctx->out);
    } else {
        xl6CgenDefaultValue(ctx, typeNode);
    }
    xl6OutputPuts(";\n", ctx->out);

    xl6CgenDeclEmitTaskSpawnArgTemps(ctx, temps, tempCapacity);

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = (size_t)tempCount;

    /*
        task.spawn 的实参预求值只解决副作用顺序问题。
        初始化表达式本身仍可能需要空合并、类型转换等语句级 lowering。
    */
    if ( xl6CgenDeclCanEmitCallArgTemp(ctx, typeNode, initExpr) ) {
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, typeNode, cType, cName, initExpr) ) {
            xl6CgenError(ctx, initExpr, "task.spawn declaration expression lowering failed");
        }
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", cName);
        xl6CgenExpr(ctx, initExpr);
        xl6OutputPuts(";\n", ctx->out);
    }

    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xrtFree(replacements);
    xl6CgenDeclFreeCallArgTemps(temps, tempCapacity);
    return 1;
}
