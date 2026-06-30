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
    if ( xl6CgenDeclTryStaticNullCoalesceCallArgTemp(ctx, typeNode, cType, cName, expr) ) {
        return 1;
    }
    if ( xl6CgenDeclTryTypeConvertCallArgTemp(ctx, typeNode, cType, cName, expr) ) {
        return 1;
    }
    return 0;
}

static int xl6CgenDeclTaskSpawnTempAppend(
    XL6_CgenContext* ctx,
    XL6_CgenDeclCallArgTemp** temps,
    int* count,
    int* capacity,
    const XL6_AstNode* expr,
    XL6_AstNode* typeNode)
{
    XL6_CgenDeclCallArgTemp* newTemps;
    int newCapacity;

    if ( ctx == NULL || temps == NULL || count == NULL || capacity == NULL || expr == NULL || typeNode == NULL ) {
        xl6AstFree(typeNode);
        return 0;
    }

    if ( *count == *capacity ) {
        newCapacity = *capacity == 0 ? 4 : (*capacity * 2);
        newTemps = (XL6_CgenDeclCallArgTemp*)xrtRealloc(*temps, (size_t)newCapacity * sizeof(XL6_CgenDeclCallArgTemp));
        if ( newTemps == NULL ) {
            xl6AstFree(typeNode);
            xl6CgenError(ctx, expr, "out of memory while preparing task.spawn argument temporaries");
            return 0;
        }
        memset(newTemps + *capacity, 0, (size_t)(newCapacity - *capacity) * sizeof(XL6_CgenDeclCallArgTemp));
        *temps = newTemps;
        *capacity = newCapacity;
    }

    (*temps)[*count].active = 1;
    (*temps)[*count].expr = expr;
    (*temps)[*count].typeNode = typeNode;
    snprintf((*temps)[*count].name, sizeof((*temps)[*count].name), "__xl6_task_arg_%d", ctx->tempId++);
    *count += 1;
    return 1;
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
        tempCType = xl6CgenTypeName(ctx, temps[i].typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPuts(tempCType, ctx->out);
        xl6OutputPrintf(ctx->out, " %s = ", temps[i].name);
        if ( xl6CgenIsXValueType(temps[i].typeNode) ) {
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6CgenDefaultValue(ctx, temps[i].typeNode);
        }
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

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", cName);
    xl6CgenExpr(ctx, initExpr);
    xl6OutputPuts(";\n", ctx->out);

    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xrtFree(replacements);
    xl6CgenDeclFreeCallArgTemps(temps, tempCapacity);
    return 1;
}

static int xl6CgenTryKnownCallStatementWithNestedClassArgTemps(XL6_CgenContext* ctx,
                                                               const XL6_AstNode* func,
                                                               const XL6_AstNode* args)
{
    XL6_CgenDeclClassArgTemp* exprTemps = NULL;
    XL6_CgenExprReplacement* exprReplacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    int exprTempCount = 0;
    int exprTempCapacity = 0;
    size_t i;

    if ( ctx == NULL || func == NULL || args == NULL ) {
        return 0;
    }

    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg != NULL && arg->childCount > 0 ) {
            xl6CgenDeclCollectClassArgTempsInExpr(ctx, arg->children[0], &exprTemps, &exprTempCount, &exprTempCapacity);
        }
    }
    if ( exprTempCount <= 0 ) {
        xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
        return 0;
    }

    exprReplacements = xl6CgenDeclCreateExprReplacements(ctx, exprTemps, exprTempCount);
    if ( exprReplacements == NULL ) {
        xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
        return 0;
    }

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    xl6CgenDeclEmitClassArgTemps(ctx, exprTemps, exprTempCount, 0);
    xl6CgenIndent(ctx);
    ctx->exprReplacements = exprReplacements;
    ctx->exprReplacementCount = (size_t)exprTempCount;
    xl6CgenDeclEmitKnownStatementCallSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    xl6CgenKnownCallArgs(ctx, func, args);
    xl6OutputPuts(");\n", ctx->out);
    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;
    xl6CgenDeclDestroyClassArgTemps(ctx, exprTemps, exprTempCount);

    xrtFree(exprReplacements);
    xl6CgenDeclFreeClassArgTemps(exprTemps, exprTempCount);
    return 1;
}

static int xl6CgenTryKnownCallStatementWithScalarArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* params;
    XL6_AstNode* inferredArgs;
    XL6_AstNode* func;
    XL6_AstNode* externFunc;
    XL6_AstNode* exportFunc;
    XL6_CgenDeclCallArgTemp* temps;
    XL6_CgenExprReplacement* replacements;
    const XL6_CgenExprReplacement* prevExprReplacements;
    size_t prevExprReplacementCount;
    int fixedCount;
    int tempCount;
    int i;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }

    inferredArgs = NULL;
    externFunc = xl6CgenFindExternFunc(ctx, callee->text);
    exportFunc = xl6CgenFindExportFunc(ctx, callee->text);
    if ( externFunc != NULL ) {
        func = externFunc;
    } else if ( exportFunc != NULL ) {
        func = exportFunc;
    } else {
        func = xl6CgenResolveCallFuncInternal(ctx, expr, callee->text, &inferredArgs, 0);
        if ( inferredArgs != NULL ) {
            xl6AstFree(inferredArgs);
            return 0;
        }
    }
    if ( func == NULL ) {
        return 0;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params == NULL || fixedCount <= 0 || params->childCount > (size_t)fixedCount ) {
        return 0;
    }

    temps = (XL6_CgenDeclCallArgTemp*)xrtMalloc((size_t)fixedCount * sizeof(XL6_CgenDeclCallArgTemp));
    if ( temps == NULL ) {
        xl6CgenError(ctx, expr, "out of memory while preparing scalar argument temporaries");
        return 0;
    }
    memset(temps, 0, (size_t)fixedCount * sizeof(XL6_CgenDeclCallArgTemp));

    args = expr->childCount > 1 ? expr->children[1] : NULL;
    tempCount = 0;
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;
        XL6_AstNode* canonicalType;

        if ( argExpr == NULL ) {
            continue;
        }

        canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, paramType);
        if ( canonicalType == NULL ) {
            continue;
        }
        if ( !xl6CgenDeclCanEmitCallArgTemp(ctx, canonicalType, argExpr) ) {
            xl6AstFree(canonicalType);
            continue;
        }

        temps[i].active = 1;
        temps[i].expr = argExpr;
        temps[i].typeNode = canonicalType;
        snprintf(temps[i].name, sizeof(temps[i].name), "__xl6_call_arg_%d", ctx->tempId++);
        tempCount += 1;
    }

    if ( tempCount == 0 ) {
        xl6CgenDeclFreeCallArgTemps(temps, fixedCount);
        return 0;
    }

    replacements = xl6CgenDeclCreateCallArgReplacements(ctx, temps, fixedCount, tempCount);
    if ( replacements == NULL ) {
        xl6CgenDeclFreeCallArgTemps(temps, fixedCount);
        return 0;
    }

    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;
        const char* cType;

        if ( !temps[i].active ) {
            continue;
        }

        cType = xl6CgenTypeName(ctx, temps[i].typeNode);
        xl6CgenIndent(ctx);
        xl6OutputPuts(cType, ctx->out);
        xl6OutputPrintf(ctx->out, " %s = ", temps[i].name);
        xl6CgenDefaultValue(ctx, temps[i].typeNode);
        xl6OutputPuts(";\n", ctx->out);
        if ( !xl6CgenDeclEmitCallArgTemp(ctx, temps[i].typeNode, cType, temps[i].name, argExpr) ) {
            xl6CgenError(ctx, argExpr, "failed to lower scalar call argument temporary");
        }
    }

    prevExprReplacements = ctx->exprReplacements;
    prevExprReplacementCount = ctx->exprReplacementCount;
    ctx->exprReplacements = replacements;
    ctx->exprReplacementCount = (size_t)tempCount;

    xl6CgenIndent(ctx);
    xl6CgenDeclEmitKnownStatementCallSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    xl6CgenKnownCallArgs(ctx, func, args);
    xl6OutputPuts(");\n", ctx->out);

    ctx->exprReplacements = prevExprReplacements;
    ctx->exprReplacementCount = prevExprReplacementCount;

    xrtFree(replacements);
    xl6CgenDeclFreeCallArgTemps(temps, fixedCount);
    return 1;
}

static int xl6CgenTryKnownCallStatementWithClassArgTemps(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* params;
    XL6_AstNode* inferredArgs;
    XL6_AstNode* func;
    XL6_AstNode* externFunc;
    XL6_AstNode* exportFunc;
    XL6_CgenDeclClassArgTemp* temps;
    int fixedCount;
    int i;
    int tempCount;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }

    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ) {
        return 0;
    }

    inferredArgs = NULL;
    externFunc = xl6CgenFindExternFunc(ctx, callee->text);
    exportFunc = xl6CgenFindExportFunc(ctx, callee->text);
    if ( externFunc != NULL ) {
        func = externFunc;
    } else if ( exportFunc != NULL ) {
        func = exportFunc;
    } else {
        func = xl6CgenResolveCallFuncInternal(ctx, expr, callee->text, &inferredArgs, 0);
        if ( inferredArgs != NULL ) {
            xl6AstFree(inferredArgs);
            return 0;
        }
    }
    if ( func == NULL ) {
        return 0;
    }

    params = func->childCount > 1 ? func->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount ) {
        return 0;
    }
    if ( fixedCount <= 0 ) {
        return 0;
    }

    temps = (XL6_CgenDeclClassArgTemp*)xrtMalloc((size_t)fixedCount * sizeof(XL6_CgenDeclClassArgTemp));
    if ( temps == NULL ) {
        xl6CgenError(ctx, expr, "out of memory while preparing class argument temporaries");
        return 0;
    }
    memset(temps, 0, (size_t)fixedCount * sizeof(XL6_CgenDeclClassArgTemp));

    args = expr->childCount > 1 ? expr->children[1] : NULL;
    tempCount = 0;
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL ? params->children[i] : NULL;
        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;
        const XL6_AstNode* paramType = param != NULL && param->childCount > 0 ? param->children[0] : NULL;

        if ( argExpr != NULL &&
             xl6CgenDeclPrepareClassArgTemp(ctx, paramType, argExpr, ctx->tempId++, &temps[i]) ) {
            tempCount += 1;
        }
    }

    if ( tempCount == 0 ) {
        xl6CgenDeclFreeClassArgTemps(temps, fixedCount);
        if ( xl6CgenTryKnownCallStatementWithNestedClassArgTemps(ctx, func, args) ) {
            return 1;
        }
        return 0;
    }

    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params != NULL ? params->children[i] : NULL;
        const XL6_AstNode* argExpr = param != NULL ? xl6CgenFindArgForParam(args, param->text, i) : NULL;

        if ( !temps[i].active ) {
            continue;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, temps[i].valueType), temps[i].valueName);
        xl6CgenEmitMemberFunctionStatementTemp(ctx, argExpr, temps[i].functionName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", temps[i].valueName);
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, argExpr, temps[i].functionType, temps[i].functionName);
        xl6OutputPuts(";\n", ctx->out);
    }

    xl6CgenIndent(ctx);
    xl6CgenDeclEmitKnownStatementCallSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    xl6CgenDeclEmitKnownCallArgsWithClassTemps(ctx, func, args, temps, fixedCount);
    xl6OutputPuts(");\n", ctx->out);

    for ( i = fixedCount - 1; i >= 0; --i ) {
        if ( temps[i].active ) {
            xl6CgenDeclDestroyTemporaryClassValue(ctx, temps[i].classNode, temps[i].valueType, temps[i].valueName);
        }
    }

    xl6CgenDeclFreeClassArgTemps(temps, fixedCount);
    return 1;
}
