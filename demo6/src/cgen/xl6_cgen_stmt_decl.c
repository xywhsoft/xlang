static void xl6CgenStmt(XL6_CgenContext* ctx, const XL6_AstNode* node);

static int xl6CgenDeclTryXValueArrayLiteralInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenDeclTryXValueNullCoalesceInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenDeclTryXValueBinaryInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenDeclTryXValueConditionalInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenDeclTryStaticXValueScalarInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr);
static int xl6CgenDeclTryXValueExprInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static void xl6CgenDeclEmitXValueExprInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);
static void xl6CgenDeclEmitXValueExprAssign(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr);

static void xl6CgenBlock(XL6_CgenContext* ctx, const XL6_AstNode* block)
{
    size_t i;
    size_t cleanupMark;

    if ( block == NULL ) {
        return;
    }
    cleanupMark = ctx->cleanupCount;
    xl6CgenPushErrorScope(ctx, cleanupMark);
    for ( i = 0; i < block->childCount; ++i ) {
        xl6CgenStmt(ctx, block->children[i]);
    }
    xl6CgenCloseErrorScope(ctx, cleanupMark);
}

static int xl6CgenDeclIsNullCoalesceExpr(const XL6_AstNode* expr)
{
    return expr != NULL && expr->kind == XL6_AST_BINARY && expr->childCount >= 2 &&
           xl6CTextEquals(expr->text, "??");
}

static int xl6CgenDeclCanLowerStaticNullCoalesceType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL ) {
        return 0;
    }

    /*
        ?? 的语句级展开会接管变量初始化与赋值时机。
        当前只处理普通标量/指针类型，复杂托管类型继续走原有专门路径。
    */
    if ( xl6CgenIsXValueType(concreteType) ||
         xl6CgenIsFunctionType(concreteType) ||
         xl6CgenIsClassType(ctx, concreteType) ||
         xl6CgenIsNativeBackedClassType(ctx, concreteType) ||
         xl6CgenIsTypedContainerType(ctx, concreteType) ||
         xl6CgenIsFutureLikeType(concreteType) ||
         xl6CgenIsChannelType(concreteType) ||
         xl6CgenIsThreadType(concreteType) ||
         xl6CgenIsGeneratorType(concreteType) ||
         xl6CgenIsSyncHandleType(concreteType) ) {
        return 0;
    }

    return 1;
}

static int xl6CgenDeclTryXValueNullCoalesceInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    int id;

    if ( ctx == NULL || typeNode == NULL || cName == NULL ||
         !xl6CgenDeclIsNullCoalesceExpr(initExpr) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    if ( left == NULL || left->kind == XL6_AST_NULL ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    if ( !xl6CgenIsXValueType(leftType) ) {
        xl6AstFree(leftType);
        return 0;
    }
    xl6AstFree(leftType);

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "xvalue* %s = 0;\n", cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    {
        char leftName[64];

        snprintf(leftName, sizeof(leftName), "__xl6_nc_%d", id);
        xl6CgenDeclEmitXValueExprInit(ctx, typeNode, leftName, left);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (xl6_xrt_value_is_null(__xl6_nc_%d)) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_nc_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xrtValueRelease(__xl6_nc_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenDeclEmitXValueExprAssign(ctx, typeNode, cName, right);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = __xl6_nc_%d;\n", cName, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryXValueBinaryInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;

    if ( ctx == NULL || typeNode == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_BINARY || initExpr->childCount < 2 ||
         !xl6CgenBinaryArithmeticIsXValue(ctx, initExpr) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    id = ctx->tempId++;

    xl6OutputPrintf(ctx->out, "xvalue* %s = 0;\n", cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    {
        char leftName[64];
        char rightName[64];

        snprintf(leftName, sizeof(leftName), "__xl6_bin_left_%d", id);
        snprintf(rightName, sizeof(rightName), "__xl6_bin_right_%d", id);
        xl6CgenDeclEmitXValueExprInit(ctx, typeNode, leftName, left);
        xl6CgenDeclEmitXValueExprInit(ctx, typeNode, rightName, right);
    }
    xl6CgenIndent(ctx);
    {
        int mode = xl6CgenXValueBinaryMode(initExpr->text);
        if ( mode == 3 || mode == 4 || mode == 8 || mode == 9 ) {
            xl6OutputPrintf(ctx->out,
                            "%s = xl6_xrt_basic_binary_checked(__xl6_bin_left_%d, __xl6_bin_right_%d, %d, %d, %d);\n",
                            cName, id, id, mode, initExpr->line, initExpr->column);
        } else {
            xl6OutputPrintf(ctx->out,
                            "%s = xl6_xrt_basic_binary(__xl6_bin_left_%d, __xl6_bin_right_%d, %d);\n",
                            cName, id, id, mode);
        }
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_bin_left_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xrtValueRelease(__xl6_bin_left_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_bin_right_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xrtValueRelease(__xl6_bin_right_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryXValueConditionalInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* cond;
    const XL6_AstNode* thenExpr;
    const XL6_AstNode* elseExpr;

    if ( ctx == NULL || typeNode == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_CONDITIONAL || initExpr->childCount < 3 ||
         !xl6CgenConditionalIsXValue(ctx, initExpr) ) {
        return 0;
    }

    cond = initExpr->children[0];
    thenExpr = initExpr->children[1];
    elseExpr = initExpr->children[2];

    xl6OutputPrintf(ctx->out, "xvalue* %s = 0;\n", cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("if (", ctx->out);
    xl6CgenConditionExpr(ctx, cond);
    xl6OutputPuts(") {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenDeclEmitXValueExprAssign(ctx, typeNode, cName, thenExpr);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenDeclEmitXValueExprAssign(ctx, typeNode, cName, elseExpr);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclStaticBoolLikeType(const XL6_AstNode* typeNode)
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

static int xl6CgenDeclTryStaticXValueScalarInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         !xl6CgenStaticScalarXValueExpr(ctx, typeNode, initExpr) ) {
        return 0;
    }

    xl6OutputPrintf(ctx->out, "%s %s = ", cType, cName);
    if ( !xl6CgenStaticScalarFromXValueExpr(ctx, typeNode, initExpr) ) {
        xl6CgenError(ctx, initExpr, "xvalue* scalar initializer lowering failed");
        xl6CgenDefaultValue(ctx, typeNode);
    }
    xl6OutputPuts(";\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryStaticXValueCompareInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;
    int equalMode;
    int negate;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_BINARY || initExpr->childCount < 2 ||
         !xl6CgenDeclStaticBoolLikeType(typeNode) ||
         xl6CgenClassProtocolBinaryAvailable(ctx, initExpr) ||
         !xl6CgenBinaryCompareIsXValue(ctx, initExpr) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    id = ctx->tempId++;
    equalMode = xl6CTextEquals(initExpr->text, "==") || xl6CTextEquals(initExpr->text, "!=");
    negate = xl6CTextEquals(initExpr->text, "!=");

    xl6OutputPrintf(ctx->out, "%s %s = 0;\n", cType, cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* __xl6_cmp_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* __xl6_cmp_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    if ( equalMode ) {
        if ( xl6CgenBinaryCompareHasClassHandle(ctx, initExpr) ) {
            xl6OutputPrintf(ctx->out,
                            "int __xl6_cmp_result_%d = "
                            "(((__xl6_cmp_left_%d == 0 || xl6_xrt_value_is_null(__xl6_cmp_left_%d)) && "
                            "(__xl6_cmp_right_%d == 0 || xl6_xrt_value_is_null(__xl6_cmp_right_%d))) || "
                            "(__xl6_cmp_left_%d != 0 && __xl6_cmp_right_%d != 0 && "
                            "!xl6_xrt_value_is_null(__xl6_cmp_left_%d) && !xl6_xrt_value_is_null(__xl6_cmp_right_%d) && "
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
                        xl6CgenXValueOrderMode(initExpr->text));
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_cmp_left_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xrtValueRelease(__xl6_cmp_left_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_cmp_right_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xrtValueRelease(__xl6_cmp_right_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
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

static int xl6CgenDeclTryStaticXValueApproxInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_BINARY || initExpr->childCount < 2 ||
         !xl6CgenDeclStaticBoolLikeType(typeNode) ||
         !xl6CgenBinaryApproxIsXValue(ctx, initExpr) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    id = ctx->tempId++;

    xl6OutputPrintf(ctx->out, "%s %s = 0;\n", cType, cName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* __xl6_approx_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* __xl6_approx_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "int __xl6_approx_result_%d = xl6_xrt_basic_approx(__xl6_approx_left_%d, __xl6_approx_right_%d);\n",
                    id,
                    id,
                    id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_approx_left_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xrtValueRelease(__xl6_approx_left_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_approx_right_%d != 0) {\n", id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xrtValueRelease(__xl6_approx_right_%d);\n", id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = (%s)(__xl6_approx_result_%d);\n", cName, cType, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static int xl6CgenDeclTryStaticNullCoalesceInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    const XL6_AstNode* nullableInner;
    int nullableTagged;
    int nullablePointer;
    int targetIsNullable;
    int targetIsInner;
    int id;

    if ( ctx == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         !xl6CgenDeclIsNullCoalesceExpr(initExpr) ||
         !xl6CgenDeclCanLowerStaticNullCoalesceType(ctx, typeNode) ) {
        return 0;
    }

    left = initExpr->children[0];
    right = initExpr->children[1];
    if ( left == NULL || right == NULL || left->kind == XL6_AST_NULL ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    nullableInner = xl6CgenNullableInnerType(leftType);
    nullableTagged = xl6CgenNullableUsesTaggedValue(leftType);
    nullablePointer = xl6CgenNullableUsesPointer(leftType);
    targetIsNullable = leftType != NULL && xl6CgenTypeNodeEqual(leftType, typeNode);
    targetIsInner = nullableInner != NULL && xl6CgenTypeNodeEqual(nullableInner, typeNode);

    if ( nullableInner != NULL && (nullableTagged || nullablePointer) &&
         (targetIsNullable || targetIsInner) ) {
        const char* leftCType = xl6CgenTypeName(ctx, leftType);

        id = ctx->tempId++;
        xl6OutputPrintf(ctx->out, "%s %s = ", cType, cName);
        xl6CgenDefaultValue(ctx, typeNode);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("{\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s __xl6_nc_%d = ", leftCType, id);
        xl6CgenExpr(ctx, left);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        if ( nullableTagged ) {
            xl6OutputPrintf(ctx->out, "if (__xl6_nc_%d.hasValue) {\n", id);
        } else {
            xl6OutputPrintf(ctx->out, "if (__xl6_nc_%d != 0) {\n", id);
        }
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", cName);
        if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
            xl6OutputPuts("xl6_nullable_string_copy(", ctx->out);
        }
        xl6OutputPrintf(ctx->out, "__xl6_nc_%d", id);
        if ( nullableTagged && targetIsInner ) {
            xl6OutputPuts(".value", ctx->out);
        }
        if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
            xl6OutputPutc(')', ctx->out);
        }
        xl6OutputPuts(";\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = ", cName);
        if ( xl6CgenIsOwnedStringTypeNode(ctx, typeNode) ) {
            xl6OutputPuts("xl6_nullable_string_copy(", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPutc(')', ctx->out);
        } else if ( targetIsNullable && nullableTagged ) {
            xl6CgenNullableTaggedValueExpr(ctx, typeNode, right);
        } else {
            xl6CgenExpr(ctx, right);
        }
        xl6OutputPuts(";\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return 1;
    }

    if ( leftType == NULL || rightType == NULL || !xl6CgenTypeNodeEqual(leftType, rightType) ) {
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return 0;
    }
    xl6AstFree(leftType);
    xl6AstFree(rightType);

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "%s %s = ", cType, cName);
    xl6CgenDefaultValue(ctx, typeNode);
    xl6OutputPuts(";\n", ctx->out);
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

static void xl6CgenDiscardTemporaryResultByType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* tempName)
{
    const XL6_AstNode* concreteType;
    const char* typeName;

    if ( ctx == NULL || typeNode == NULL || tempName == NULL || tempName[0] == '\0' ) {
        return;
    }

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    typeName = concreteType != NULL ? concreteType->text : NULL;
    if ( concreteType == NULL || typeName == NULL ) {
        return;
    }

    if ( xl6CgenIsErrorType(ctx, concreteType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xl6_error_free(%s); %s = 0; }\n", tempName, tempName, tempName);
        return;
    }

    if ( xl6CgenIsTypedContainerType(ctx, concreteType) ) {
        const XL6_AstNode* containerType = xl6CgenTypedContainerConcreteType(ctx, concreteType);
        const char* containerName = containerType != NULL ? containerType->text : NULL;

        xl6CgenIndent(ctx);
        if ( xl6CTextEquals(containerName, "array") ) {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xl6TypedArrayRelease(%s); %s = 0; }\n", tempName, tempName, tempName);
        } else if ( xl6CTextEquals(containerName, "list") ) {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xl6TypedListRelease(%s); %s = 0; }\n", tempName, tempName, tempName);
        } else if ( xl6CTextEquals(containerName, "set") ) {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xl6TypedSetRelease(%s); %s = 0; }\n", tempName, tempName, tempName);
        } else if ( xl6CTextEquals(containerName, "dict") ) {
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xl6TypedDictRelease(%s); %s = 0; }\n", tempName, tempName, tempName);
        }
        return;
    }

    if ( xl6CgenIsSpecialCollectionType(ctx, concreteType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { ", tempName);
        xl6CgenSpecialCollectionReleaseCExpr(ctx, concreteType, tempName);
        xl6OutputPrintf(ctx->out, "; %s = 0; }\n", tempName);
        return;
    }

    if ( xl6CgenIsXValueType(concreteType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRelease(%s); %s = 0; }\n", tempName, tempName, tempName);
        return;
    }

    if ( xl6CgenIsFunctionType(concreteType) ) {
        xl6CgenReleaseFunctionPath(ctx, tempName);
        return;
    }

    if ( xl6CgenIsNativeBackedClassType(ctx, concreteType) ) {
        const XL6_AstNode* nativeClassNode = xl6CgenFindNativeBackedClass(ctx, typeName);
        const XL6_AstNode* destroyNode = nativeClassNode != NULL ? xl6CgenFindNativeBackedClassSpecial(nativeClassNode, "destroy") : NULL;

        if ( destroyNode != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s.handle != 0) { ", tempName);
            xl6CgenCStringSymbol(ctx->out, destroyNode->text);
            xl6OutputPrintf(ctx->out, "(%s.handle); %s.handle = 0; }\n", tempName, tempName);
        }
        return;
    }

    if ( xl6CgenIsClassType(ctx, concreteType) ) {
        XL6_AstNode* classNode = xl6CgenClassNodeForType(ctx, concreteType);

        xl6CgenDeclDestroyTemporaryClassValue(ctx, classNode, concreteType, tempName);
        return;
    }

    /* 标量临时量承载语句级 lowering 的结果，显式标记其结果被有意丢弃。 */
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "(void)%s;\n", tempName);
}

static void xl6CgenDiscardTemporaryMultiResultByTypes(XL6_CgenContext* ctx, const XL6_AstNode* returns, const char* tempName)
{
    size_t i;

    if ( ctx == NULL || returns == NULL || tempName == NULL || tempName[0] == '\0' ) {
        return;
    }

    for ( i = 0; i < returns->childCount; ++i ) {
        char valuePath[128];

        snprintf(valuePath, sizeof(valuePath), "%s.value%u", tempName, (unsigned)i);
        xl6CgenDiscardTemporaryResultByType(ctx, returns->children[i], valuePath);
    }
}

static int xl6CgenTryMemberFunctionStatementInvoke(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* returns;
    const XL6_AstNode* returnType;
    XL6_AstNode* functionType;
    char tempName[64];
    char resultName[64];
    int id;

    if ( ctx == NULL || expr == NULL || expr->kind != XL6_AST_CALL || expr->childCount == 0 ) {
        return 0;
    }
    callee = expr->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ) {
        return 0;
    }

    functionType = xl6CgenCloneMemberFieldTypeForInference(ctx, callee);
    returns = xl6CgenFunctionReturnTypes(functionType);
    if ( !xl6CgenIsFunctionType(functionType) ||
         returns == NULL ||
         xl6CgenFunctionTypeSupportsErasedCall(ctx, functionType) ) {
        xl6AstFree(functionType);
        return 0;
    }

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_member_fn_stmt_%d", id);
    xl6CgenEmitMemberFunctionStatementTemp(ctx, expr, tempName);
    xl6CgenIndent(ctx);
    if ( returns->childCount == 0 ) {
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, expr, functionType, tempName);
        xl6OutputPuts(";\n", ctx->out);
    } else if ( returns->childCount == 1 ) {
        returnType = returns->children[0];
        snprintf(resultName, sizeof(resultName), "__xl6_member_result_%d", id);
        xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, returnType), resultName);
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, expr, functionType, tempName);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenDiscardTemporaryResultByType(ctx, returnType, resultName);
    } else {
        snprintf(resultName, sizeof(resultName), "__xl6_member_result_%d", id);
        xl6CgenFunctionReturnStructName(ctx,
                                        xl6CgenFunctionParamTypes(functionType),
                                        returns);
        xl6OutputPrintf(ctx->out, " %s = ", resultName);
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, expr, functionType, tempName);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenDiscardTemporaryMultiResultByTypes(ctx, returns, resultName);
    }
    xl6AstFree(functionType);
    return 1;
}

static int xl6CgenDeclTryTemporaryClassFieldInit(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node,
    const XL6_AstNode* typeNode,
    const char* cType,
    const char* cName,
    const XL6_AstNode* initExpr)
{
    const XL6_AstNode* receiver;
    XL6_AstNode* receiverType;
    XL6_AstNode* fieldType;
    XL6_AstNode* classNode;
    XL6_AstNode* memberFunctionType;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    int id;
    char receiverTempName[64];
    char memberTempName[64];

    if ( ctx == NULL || node == NULL || typeNode == NULL || cType == NULL || cName == NULL ||
         initExpr == NULL || initExpr->kind != XL6_AST_MEMBER || initExpr->childCount == 0 ) {
        return 0;
    }

    receiver = initExpr->children[0];
    if ( receiver == NULL || receiver->kind != XL6_AST_CALL ) {
        return 0;
    }

    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    classNode = xl6CgenClassNodeForType(ctx, receiverType);
    if ( classNode == NULL ) {
        xl6AstFree(receiverType);
        return 0;
    }

    fieldType = xl6CgenCloneMemberFieldTypeForInference(ctx, initExpr);
    if ( fieldType == NULL || !xl6CgenDeclTemporaryClassFieldTypeIsPlain(ctx, fieldType) ) {
        xl6AstFree(fieldType);
        xl6AstFree(receiverType);
        return 0;
    }

    id = ctx->tempId++;
    snprintf(receiverTempName, sizeof(receiverTempName), "__xl6_class_field_recv_%d", id);
    snprintf(memberTempName, sizeof(memberTempName), "__xl6_member_fn_stmt_%d", id);

    xl6OutputPrintf(ctx->out, "%s %s = {0};\n", xl6CgenTypeName(ctx, receiverType), receiverTempName);

    memberFunctionType = xl6CgenCloneMemberFunctionCallTypeForStatementInvoke(ctx, receiver);
    if ( memberFunctionType != NULL ) {
        xl6CgenEmitMemberFunctionStatementTemp(ctx, receiver, memberTempName);
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = ", receiverTempName);
    if ( memberFunctionType != NULL ) {
        xl6CgenMemberFunctionStatementInvokeExpr(ctx, receiver, memberFunctionType, memberTempName);
    } else {
        xl6CgenExpr(ctx, receiver);
    }
    xl6OutputPuts(";\n", ctx->out);

    xl6CgenAddVarType(ctx, node->text, typeNode);
    xl6CgenAddFunctionCleanup(ctx, node->text, typeNode);
    xl6CgenAddFutureCleanup(ctx, node->text, typeNode);
    xl6CgenAddChannelCleanup(ctx, node->text, typeNode);
    xl6CgenAddThreadCleanup(ctx, node->text, typeNode);
    xl6CgenAddGeneratorCleanup(ctx, node->text, typeNode);
    xl6CgenAddSyncCleanup(ctx, node->text, typeNode);

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( receiverType->childCount > 0 && xl6CgenClassHasTypeParams(classNode) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = receiverType->children[0];
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", cType, cName);
    if ( !xl6CgenEmitClassFieldFromNamedReceiver(
             ctx,
             receiverType,
             classNode,
             receiverTempName,
             initExpr->text,
             initExpr) ) {
        xl6CgenError(ctx, initExpr, "class field not found");
    }
    xl6OutputPuts(";\n", ctx->out);

    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;

    xl6CgenDeclDestroyTemporaryClassValue(ctx, classNode, receiverType, receiverTempName);

    xl6AstFree(memberFunctionType);
    xl6AstFree(fieldType);
    xl6AstFree(receiverType);
    return 1;
}
