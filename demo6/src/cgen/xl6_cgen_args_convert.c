static void xl6CgenPrintXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawTypeNode;
    const XL6_AstNode* typeNode;

    rawTypeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawTypeNode));
    if ( typeNode != NULL && xl6CgenIsUnsignedIntegerTypeName(typeNode->text) ) {
        /* xvalue 的 int 是有符号 int64；打印无符号整数时先保留完整文本。 */
        xl6OutputPuts("xvoCreateText((void*)xl6_xrt_u64_to_string((unsigned long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(")), 0, false)", ctx->out);
    } else {
        xl6CgenXValueFromExpr(ctx, expr);
    }
    xl6AstFree(rawTypeNode);
}

static int xl6CgenExprIsXValueResult(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* typeNode;
    int result;

    typeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    result = xl6CgenIsXValueType(typeNode);
    xl6AstFree(typeNode);
    return result;
}

static void xl6CgenConditionExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( !xl6CgenExprIsXValueResult(ctx, expr) ) {
        xl6CgenExpr(ctx, expr);
        return;
    }

    xl6OutputPuts("xl6_xrt_value_truthy_release(", ctx->out);
    xl6CgenXValueFromExpr(ctx, expr);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenConditionalIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node == NULL || node->kind != XL6_AST_CONDITIONAL || node->childCount < 3 ) {
        return 0;
    }
    return xl6CgenExprIsXValueResult(ctx, node);
}

static void xl6CgenXValueConditionalExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node == NULL || node->childCount < 3 ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }

    xl6OutputPutc('(', ctx->out);
    xl6CgenConditionExpr(ctx, node->children[0]);
    xl6OutputPuts(" ? ", ctx->out);
    xl6CgenXValueFromExpr(ctx, node->children[1]);
    xl6OutputPuts(" : ", ctx->out);
    xl6CgenXValueFromExpr(ctx, node->children[2]);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenBinaryCompareOpIsXValue(const char* op)
{
    return xl6CTextEquals(op, "==") || xl6CTextEquals(op, "!=") ||
           xl6CTextEquals(op, "<") || xl6CTextEquals(op, "<=") ||
           xl6CTextEquals(op, ">") || xl6CTextEquals(op, ">=");
}

static int xl6CgenIsBasicXValueType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6CgenIsXValueType(typeNode) && xl6CgenClassHandleTargetType(ctx, typeNode) == NULL;
}

static int xl6CgenBinaryCompareHasClassHandle(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int result;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ||
         (!xl6CTextEquals(node->text, "==") && !xl6CTextEquals(node->text, "!=")) ) {
        return 0;
    }
    leftType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
    rightType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
    result = xl6CgenClassHandleTargetType(ctx, leftType) != NULL ||
             xl6CgenClassHandleTargetType(ctx, rightType) != NULL;
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return result;
}

static int xl6CgenBinaryCompareIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int typedContainerBool;
    int result;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ||
         !xl6CgenBinaryCompareOpIsXValue(node->text) ) {
        return 0;
    }

    typedContainerBool = 0;
    if ( xl6CgenTypedContainerBinaryInfo(ctx, node, NULL, &typedContainerBool) && typedContainerBool ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
    rightType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
    result = xl6CgenBinaryCompareHasClassHandle(ctx, node) ||
             xl6CgenIsBasicXValueType(ctx, leftType) ||
             xl6CgenIsBasicXValueType(ctx, rightType);
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return result;
}

static int xl6CgenXValueOrderMode(const char* op)
{
    if ( xl6CTextEquals(op, "<") ) {
        return 0;
    }
    if ( xl6CTextEquals(op, "<=") ) {
        return 1;
    }
    if ( xl6CTextEquals(op, ">") ) {
        return 2;
    }
    return 3;
}

static void xl6CgenXValueCompareExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node == NULL || node->childCount < 2 ) {
        xl6OutputPuts("0", ctx->out);
        return;
    }

    if ( xl6CTextEquals(node->text, "==") || xl6CTextEquals(node->text, "!=") ) {
        if ( xl6CTextEquals(node->text, "!=") ) {
            xl6OutputPuts("(!", ctx->out);
        }
        xl6OutputPuts(
            xl6CgenBinaryCompareHasClassHandle(ctx, node) ?
                "xl6_class_identity_equal_release(" :
                "xl6_xrt_basic_equal_release(",
            ctx->out);
        xl6CgenXValueFromExpr(ctx, node->children[0]);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, node->children[1]);
        xl6OutputPutc(')', ctx->out);
        if ( xl6CTextEquals(node->text, "!=") ) {
            xl6OutputPutc(')', ctx->out);
        }
        return;
    }

    xl6OutputPuts("xl6_xrt_basic_order_release(", ctx->out);
    xl6CgenXValueFromExpr(ctx, node->children[0]);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenXValueFromExpr(ctx, node->children[1]);
    xl6OutputPrintf(ctx->out, ", %d)", xl6CgenXValueOrderMode(node->text));
}
static int xl6CgenBinaryApproxIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int result;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ||
         !xl6CTextEquals(node->text, "~=") ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
    rightType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
    result = xl6CgenIsXValueType(leftType) || xl6CgenIsXValueType(rightType);
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return result;
}

static void xl6CgenXValueApproxExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node == NULL || node->childCount < 2 ) {
        xl6OutputPuts("0", ctx->out);
        return;
    }

    xl6OutputPuts("xl6_xrt_basic_approx_release(", ctx->out);
    xl6CgenXValueFromExpr(ctx, node->children[0]);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenXValueFromExpr(ctx, node->children[1]);
    xl6OutputPutc(')', ctx->out);
}
static int xl6CgenBinaryArithmeticOpIsXValue(const char* op)
{
    return xl6CTextEquals(op, "+") || xl6CTextEquals(op, "-") ||
           xl6CTextEquals(op, "*") || xl6CTextEquals(op, "/") ||
           xl6CTextEquals(op, "%") || xl6CTextEquals(op, "&") ||
           xl6CTextEquals(op, "|") || xl6CTextEquals(op, "^") ||
           xl6CTextEquals(op, "<<") || xl6CTextEquals(op, ">>");
}

static int xl6CgenBinaryArithmeticIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int result;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ||
         !xl6CgenBinaryArithmeticOpIsXValue(node->text) ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
    rightType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
    result = xl6CgenIsBasicXValueType(ctx, leftType) ||
             xl6CgenIsBasicXValueType(ctx, rightType);
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return result;
}

static int xl6CgenXValueBinaryMode(const char* op)
{
    if ( xl6CTextEquals(op, "+") ) {
        return 0;
    }
    if ( xl6CTextEquals(op, "-") ) {
        return 1;
    }
    if ( xl6CTextEquals(op, "*") ) {
        return 2;
    }
    if ( xl6CTextEquals(op, "/") ) {
        return 3;
    }
    if ( xl6CTextEquals(op, "%") ) {
        return 4;
    }
    if ( xl6CTextEquals(op, "&") ) {
        return 5;
    }
    if ( xl6CTextEquals(op, "|") ) {
        return 6;
    }
    if ( xl6CTextEquals(op, "^") ) {
        return 7;
    }
    if ( xl6CTextEquals(op, "<<") ) {
        return 8;
    }
    return 9;
}

static void xl6CgenXValueBinaryExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int mode;

    if ( node == NULL || node->childCount < 2 ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }

    left = node->children[0];
    right = node->children[1];
    mode = xl6CgenXValueBinaryMode(node->text);
    xl6OutputPuts((mode == 3 || mode == 4 || mode == 8 || mode == 9) ?
                  "xl6_xrt_basic_binary_release_checked(" :
                  "xl6_xrt_basic_binary_release(", ctx->out);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenXValueFromExpr(ctx, right);
    if ( mode == 3 || mode == 4 || mode == 8 || mode == 9 ) {
        xl6OutputPrintf(ctx->out, ", %d, %d, %d)", mode, node->line, node->column);
    } else {
        xl6OutputPrintf(ctx->out, ", %d)", mode);
    }
}

static int xl6CgenUnaryIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* valueType;
    int result;

    if ( node == NULL || node->kind != XL6_AST_UNARY || node->childCount == 0 ) {
        return 0;
    }

    valueType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
    result = xl6CgenIsBasicXValueType(ctx, valueType);
    xl6AstFree(valueType);
    return result;
}

static int xl6CgenXValueUnaryMode(const char* op)
{
    if ( xl6CTextEquals(op, "+") ) {
        return 0;
    }
    if ( xl6CTextEquals(op, "-") ) {
        return 1;
    }
    return 2;
}

static void xl6CgenXValueUnaryExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* value;

    value = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    xl6OutputPuts("xl6_xrt_basic_unary_release(", ctx->out);
    xl6CgenXValueFromExpr(ctx, value);
    xl6OutputPrintf(ctx->out,
                     ", %d)",
                     xl6CgenXValueUnaryMode(node != NULL ? node->text : "+"));
}

static void xl6CgenXValueNotExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* value;

    value = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    xl6OutputPuts("(!xl6_xrt_value_truthy_release(", ctx->out);
    xl6CgenXValueFromExpr(ctx, value);
    xl6OutputPuts("))", ctx->out);
}
static int xl6CgenBinaryLogicalIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int result;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ||
         (!xl6CTextEquals(node->text, "and") && !xl6CTextEquals(node->text, "or")) ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
    rightType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
    result = xl6CgenIsXValueType(leftType) || xl6CgenIsXValueType(rightType);
    xl6AstFree(leftType);
    xl6AstFree(rightType);
    return result;
}

static void xl6CgenXValueLogicalExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    if ( node == NULL || node->childCount < 2 ) {
        xl6OutputPuts("0", ctx->out);
        return;
    }

    xl6OutputPutc('(', ctx->out);
    xl6CgenConditionExpr(ctx, node->children[0]);
    xl6OutputPuts(xl6CTextEquals(node->text, "and") ? " && " : " || ", ctx->out);
    xl6CgenConditionExpr(ctx, node->children[1]);
    xl6OutputPutc(')', ctx->out);
}
static const XL6_AstNode* xl6CgenTypeConvertArg(const XL6_AstNode* node)
{
    const XL6_AstNode* args;
    const XL6_AstNode* arg;

    args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    arg = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
    return arg != NULL && arg->childCount > 0 ? arg->children[0] : NULL;
}

static void xl6CgenCastExpr(XL6_CgenContext* ctx, const char* cType, const XL6_AstNode* expr)
{
    xl6OutputPrintf(ctx->out, "((%s)(", cType != NULL ? cType : "int");
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts("))", ctx->out);
}

static void xl6CgenXValueScalarGetExpr(XL6_CgenContext* ctx, const char* cType, const char* getter, const XL6_AstNode* expr)
{
    const char* releaseHelper;

    if ( xl6CgenExprIsXValueBorrow(ctx, expr) ) {
        xl6OutputPrintf(ctx->out, "((%s)%s(", cType != NULL ? cType : "int", getter != NULL ? getter : "xvoGetInt");
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
        return;
    }

    releaseHelper = NULL;
    if ( getter == NULL || xl6CTextEquals(getter, "xvoGetInt") ) {
        releaseHelper = "xl6_xrt_value_to_i64_release";
    } else if ( xl6CTextEquals(getter, "xvoGetFloat") ) {
        releaseHelper = "xl6_xrt_value_to_f64_release";
    } else if ( xl6CTextEquals(getter, "xvoGetBool") ) {
        releaseHelper = "xl6_xrt_value_to_bool_release";
    } else if ( xl6CTextEquals(getter, "xvoGetTime") ) {
        releaseHelper = "xl6_xrt_value_to_time_release";
    } else if ( xl6CTextEquals(getter, "xvoGetPoint") ) {
        releaseHelper = "xl6_xrt_value_to_point_release";
    }

    if ( releaseHelper != NULL ) {
        xl6OutputPrintf(ctx->out, "((%s)%s(", cType != NULL ? cType : "int", releaseHelper);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
        return;
    }

    xl6CgenError(ctx, expr, "unsupported xvalue scalar getter in C generator");
    xl6OutputPrintf(ctx->out, "((%s)0)", cType != NULL ? cType : "int");
}

static void xl6CgenClassToStringSelfExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* arg,
    const XL6_AstNode* sourceType,
    const XL6_AstNode* classType)
{
    if ( xl6CgenClassHandleTargetType(ctx, sourceType) != NULL ) {
        char cTypeName[512];

        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), classType);
        xl6OutputPrintf(ctx->out, "((%s*)xl6_class_data(", cTypeName);
        xl6CgenExpr(ctx, arg);
        xl6OutputPrintf(ctx->out, ", \"\", %d, %d))", arg != NULL ? arg->line : 0, arg != NULL ? arg->column : 0);
        return;
    }
    if ( arg != NULL && arg->kind == XL6_AST_NAME &&
         xl6CTextEquals(arg->text, "self") && ctx->currentClass != NULL ) {
        xl6OutputPuts("self", ctx->out);
    } else {
        xl6OutputPutc('&', ctx->out);
        if ( arg != NULL && arg->kind == XL6_AST_NAME ) {
            xl6CgenNamedValueReference(ctx, arg->text);
        } else {
            xl6OutputPutc('(', ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPutc(')', ctx->out);
        }
    }
}

static int xl6CgenClassToStringConversionExpr(XL6_CgenContext* ctx, const XL6_AstNode* arg, const XL6_AstNode* sourceType)
{
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    const XL6_AstNode* method;

    concreteType = xl6CgenResolveAliasType(ctx, sourceType);
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    classNode = concreteType != NULL && concreteType->text != NULL ? xl6CgenFindClass(ctx, concreteType->text) : NULL;
    if ( classNode == NULL ) {
        return 0;
    }

    method = xl6CgenFindClassFunc(classNode, "__toString__");
    if ( method != NULL ) {
        if ( xl6CgenClassProtocolCallPrefix(ctx, arg, classNode, classNode, method, sourceType) ) {
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6OutputPuts("\"\"", ctx->out);
        }
        return 1;
    }
    if ( !xl6CgenClassHasTypeParams(classNode) && xl6CgenClassHasDerive(classNode, "string") ) {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__toString__");
        xl6OutputPutc('(', ctx->out);
        xl6CgenClassToStringSelfExpr(ctx, arg, sourceType, concreteType);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }
    return 0;
}

static void xl6CgenTypeConvertExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* arg;
    XL6_AstNode* sourceType;
    const char* target;
    const char* source;
    int sourceIsXValue;
    XL6_CgenIntTypeInfo targetInt;

    callee = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    arg = xl6CgenTypeConvertArg(node);
    target = callee != NULL ? callee->text : NULL;
    sourceType = xl6CgenCloneExprTypeForInference(ctx, arg);
    source = sourceType != NULL ? sourceType->text : NULL;
    sourceIsXValue = xl6CgenIsXValueType(sourceType);

    if ( xl6CTextEquals(target, "var") ) {
        xl6CgenXValueFromExpr(ctx, arg);
    } else if ( sourceIsXValue ) {
        if ( xl6CgenIsXValueTypeName(target) ) {
            xl6CgenXValueFromExpr(ctx, arg);
        } else if ( xl6CgenIsIntegerTypeName(target) ) {
            xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetInt", arg);
        } else if ( xl6CgenIsFloatTypeName(target) ) {
            xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetFloat", arg);
        } else if ( xl6CgenIsBoolTypeName(target) ) {
            xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetBool", arg);
        } else if ( xl6CTextEquals(target, "string") ) {
            if ( xl6CgenClassToStringConversionExpr(ctx, arg, sourceType) ) {
                ;
            } else {
                if ( xl6CgenExprIsXValueBorrow(ctx, arg) ) {
                    xl6OutputPuts("xl6_xrt_value_to_string(", ctx->out);
                } else {
                    xl6OutputPuts("xl6_xrt_value_to_string_release(", ctx->out);
                }
                xl6CgenExpr(ctx, arg);
                xl6OutputPutc(')', ctx->out);
            }
        } else if ( xl6CTextEquals(target, "time") ) {
            xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetTime", arg);
        } else if ( xl6CTextEquals(target, "point") ) {
            xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetPoint", arg);
        } else {
            char message[256];
            snprintf(message, sizeof(message),
                     "C 代码生成不支持从 %s 转换为 %s",
                     source != NULL ? source : "<unknown>",
                     target != NULL ? target : "<unknown>");
            xl6CgenError(ctx, node, message);
            xl6OutputPuts("0", ctx->out);
        }
    } else if ( xl6CgenIsIntegerTypeName(target) ) {
        xl6OutputPrintf(ctx->out, "((%s)(", xl6CgenConvertCTypeName(target));
        if ( xl6CTextEquals(source, "string") ) {
            xl6OutputPrintf(ctx->out, "xl6_xrt_str_to_i64_checked(");
            xl6CgenExpr(ctx, arg);
            xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", node->line, node->column);
        } else if ( xl6CgenIsFloatTypeName(source) && xl6CgenIntTypeInfo(target, &targetInt) ) {
            xl6OutputPuts(targetInt.isSigned ?
                              "xl6_xrt_f64_to_i64_checked((double)(" :
                              "xl6_xrt_f64_to_u64_checked((double)(",
                          ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPrintf(ctx->out,
                            "), %d, \"\", %d, %d)",
                            targetInt.bits,
                            node->line,
                            node->column);
        } else if ( xl6CTextEquals(source, "null") ) {
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6CgenExpr(ctx, arg);
        }
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsFloatTypeName(target) ) {
        xl6OutputPrintf(ctx->out, "((%s)(", xl6CgenConvertCTypeName(target));
        if ( xl6CTextEquals(source, "string") ) {
            xl6OutputPuts("xl6_xrt_str_to_f64_checked(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPrintf(ctx->out, ", \"\", %d, %d)", node->line, node->column);
        } else if ( xl6CTextEquals(source, "null") ) {
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6CgenExpr(ctx, arg);
        }
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsBoolTypeName(target) ) {
        if ( xl6CTextEquals(source, "null") ) {
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6OutputPutc('(', ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts(" != 0)", ctx->out);
        }
    } else if ( xl6CTextEquals(target, "string") ) {
        if ( xl6CTextEquals(source, "string") ) {
            xl6CgenExpr(ctx, arg);
        } else if ( xl6CTextEquals(source, "null") ) {
            xl6OutputPuts("\"\"", ctx->out);
        } else if ( xl6CgenIsIntegerTypeName(source) ) {
            xl6OutputPuts(xl6CgenIsUnsignedIntegerTypeName(source) ?
                          "xl6_xrt_u64_to_string((unsigned long long)(" :
                          "xl6_xrt_i64_to_string((long long)(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CgenIsFloatTypeName(source) ) {
            xl6OutputPuts("xl6_xrt_f64_to_string((double)(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CgenIsBoolTypeName(source) ) {
            xl6OutputPutc('(', ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts(" ? \"true\" : \"false\")", ctx->out);
        } else if ( xl6CTextEquals(source, "type") ) {
            xl6OutputPuts("xl6_program_type_name_from_id((unsigned long long)(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CTextEquals(source, "time") ) {
            xl6OutputPuts("xl6_xrt_take_cstr((char*)xrtTimeToStr((long long)(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("), 0))", ctx->out);
        } else if ( xl6CTextEquals(source, "point") ) {
            xl6OutputPuts("xl6_xrt_take_cstr((char*)xrtFormat(\"[point:%p]\", ", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CgenClassToStringConversionExpr(ctx, arg, sourceType) ) {
            ;
        } else {
            xl6CgenError(ctx, node, "当前 C 生成不支持转换为 string");
            xl6OutputPuts("\"\"", ctx->out);
        }
    } else if ( xl6CTextEquals(target, "time") ) {
        if ( xl6CTextEquals(source, "string") ) {
            xl6OutputPuts("xrtStrToTime(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts(", 0)", ctx->out);
        } else if ( xl6CTextEquals(source, "null") ) {
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6CgenCastExpr(ctx, "long long", arg);
        }
    } else if ( xl6CTextEquals(target, "point") ) {
        if ( xl6CTextEquals(source, "null") ) {
            xl6OutputPuts("(void*)0", ctx->out);
        } else if ( xl6CTextEquals(source, "point") ) {
            xl6CgenExpr(ctx, arg);
        } else {
            xl6CgenError(ctx, node, "当前 C 生成不支持转换为 point");
            xl6OutputPuts("(void*)0", ctx->out);
        }
    } else {
        char message[256];
        snprintf(message, sizeof(message),
                 "C 代码生成不支持从 %s 转换为 %s",
                 source != NULL ? source : "<unknown>",
                 target != NULL ? target : "<unknown>");
        xl6CgenError(ctx, node, message);
        xl6OutputPuts("0", ctx->out);
    }

    xl6AstFree(sourceType);
}
