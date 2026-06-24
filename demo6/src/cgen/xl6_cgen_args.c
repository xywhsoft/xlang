static int xl6CgenParamIsVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           (xl6CTextEquals(param->children[0]->text, "array<var>") ||
            xl6CTextEquals(param->children[0]->text, "dict<var>"));
}

static int xl6CgenParamIsArrayVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6CTextEquals(param->children[0]->text, "array<var>");
}

static int xl6CgenParamIsDictVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6CTextEquals(param->children[0]->text, "dict<var>");
}

static int xl6CgenFixedParamCount(const XL6_AstNode* params)
{
    int count;

    count = 0;
    if ( params == NULL ) {
        return 0;
    }
    while ( (size_t)count < params->childCount && !xl6CgenParamIsVararg(params->children[count]) ) {
        ++count;
    }
    return count;
}

static int xl6CgenParamRequiredCount(const XL6_AstNode* params)
{
    int count;

    count = 0;
    if ( params == NULL ) {
        return 0;
    }
    while ( (size_t)count < params->childCount && !xl6CgenParamIsVararg(params->children[count]) ) {
        if ( params->children[count]->childCount > 1 ) {
            break;
        }
        ++count;
    }
    return count;
}

static int xl6CgenFuncHasVararg(const XL6_AstNode* funcNode)
{
    const XL6_AstNode* params;
    int fixedCount;

    if ( funcNode == NULL || funcNode->childCount <= 1 ) {
        return 0;
    }
    params = funcNode->children[1];
    fixedCount = xl6CgenFixedParamCount(params);
    return params != NULL && params->childCount > (size_t)fixedCount;
}

static const XL6_AstNode* xl6CgenArgExpr(const XL6_AstNode* arg)
{
    if ( arg != NULL && arg->childCount > 0 ) {
        return arg->children[0];
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindArgForParam(const XL6_AstNode* args, const char* name, int index)
{
    size_t i;
    int positionalIndex;

    if ( args == NULL ) {
        return NULL;
    }

    positionalIndex = 0;
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            if ( xl6CTextEquals(arg->text, name) ) {
                return xl6CgenArgExpr(arg);
            }
        } else {
            if ( positionalIndex == index ) {
                return xl6CgenArgExpr(arg);
            }
            positionalIndex += 1;
        }
    }
    return NULL;
}

static int xl6CgenFindParamIndex(const XL6_AstNode* params, int fixedCount, const char* name)
{
    int i;

    if ( params == NULL || name == NULL ) {
        return -1;
    }
    for ( i = 0; i < fixedCount && (size_t)i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && xl6CTextEquals(param->text, name) ) {
            return i;
        }
    }
    return -1;
}

static void xl6CgenWriteCStringChar(XL6_Output* out, unsigned char ch)
{
    switch ( ch ) {
    case '\n':
        xl6OutputPuts("\\n", out);
        break;
    case '\r':
        xl6OutputPuts("\\r", out);
        break;
    case '\t':
        xl6OutputPuts("\\t", out);
        break;
    case '"':
    case '\\':
        xl6OutputPutc('\\', out);
        xl6OutputPutc((int)ch, out);
        break;
    default:
        xl6OutputPutc((int)ch, out);
        break;
    }
}

static void xl6CgenCStringLiteral(XL6_Output* out, const char* text)
{
    const unsigned char* p;

    xl6OutputPutc('"', out);
    if ( text != NULL ) {
        for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
            xl6CgenWriteCStringChar(out, *p);
        }
    }
    xl6OutputPutc('"', out);
}

static int xl6CgenStringLiteralParts(const char* text, size_t* outStart, size_t* outEnd, int* outRaw)
{
    size_t len;
    size_t pos;
    int raw;

    if ( text == NULL || outStart == NULL || outEnd == NULL || outRaw == NULL ) {
        return 0;
    }
    len = strlen(text);
    pos = 0;
    raw = 0;
    if ( len > 0 && text[0] == 'r' ) {
        raw = 1;
        pos = 1;
    }

    if ( pos + 6 <= len && strncmp(text + pos, "\"\"\"", 3) == 0 &&
         strncmp(text + len - 3, "\"\"\"", 3) == 0 ) {
        *outStart = pos + 3;
        *outEnd = len - 3;
        *outRaw = raw;
        return 1;
    }
    if ( pos + 6 <= len && strncmp(text + pos, "'''", 3) == 0 &&
         strncmp(text + len - 3, "'''", 3) == 0 ) {
        *outStart = pos + 3;
        *outEnd = len - 3;
        *outRaw = raw;
        return 1;
    }
    if ( pos + 2 <= len && (text[pos] == '"' || text[pos] == '\'') && text[len - 1] == text[pos] ) {
        *outStart = pos + 1;
        *outEnd = len - 1;
        *outRaw = raw;
        return 1;
    }
    return 0;
}

static void xl6CgenStringLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* text;
    size_t start;
    size_t end;
    size_t i;
    int raw;

    text = node != NULL ? node->text : NULL;
    if ( !xl6CgenStringLiteralParts(text, &start, &end, &raw) ) {
        xl6CgenError(ctx, node, "字符串字面量格式无效");
        xl6OutputPuts("\"\"", ctx->out);
        return;
    }

    xl6OutputPutc('"', ctx->out);
    for ( i = start; i < end; ++i ) {
        unsigned char ch = (unsigned char)text[i];
        if ( !raw && ch == '\\' && i + 1 < end ) {
            xl6OutputPutc('\\', ctx->out);
            xl6OutputPutc(text[++i], ctx->out);
            continue;
        }
        xl6CgenWriteCStringChar(ctx->out, ch);
    }
    xl6OutputPutc('"', ctx->out);
}

static int xl6CgenTypedContainerXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawTypeNode;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const char* typeName;

    if ( !xl6CgenExprUsesTypedContainerStorage(ctx, expr) ) {
        return 0;
    }

    rawTypeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawTypeNode));
    concreteType = xl6CgenTypedContainerConcreteType(ctx, typeNode);
    itemType = xl6CgenTypedContainerItemType(ctx, typeNode);
    typeName = concreteType != NULL ? concreteType->text : NULL;

    if ( typeName == NULL || itemType == NULL ) {
        xl6CgenError(ctx, expr, "typed container boxing requires a concrete item type");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }

    xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
    if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_to_xvalue(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_to_xvalue(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_to_xvalue(", ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_to_xvalue(", ctx->out);
    } else {
        xl6CgenError(ctx, expr, "typed container boxing requires array/list/set/dict");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts("), ", ctx->out);
    xl6CgenEmitRuntimeTypeDescRef(ctx, typeNode);
    xl6OutputPutc(')', ctx->out);

    xl6AstFree(rawTypeNode);
    return 1;
}
static void xl6CgenXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawTypeNode;
    const XL6_AstNode* typeNode;
    char cTypeName[512];

    if ( expr == NULL ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }
    if ( expr->kind == XL6_AST_NULL ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }
    if ( xl6CgenTypedContainerXValueFromExpr(ctx, expr) ) {
        return;
    }

    rawTypeNode = xl6CgenCloneExprTypeForInference(ctx, expr);
    typeNode = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, rawTypeNode));
    if ( typeNode != NULL && xl6CgenIsXValueType(typeNode) ) {
        if ( xl6CgenExprIsXValueBorrow(ctx, expr) ) {
            xl6OutputPuts("xl6_xrt_retain(", ctx->out);
            xl6CgenExpr(ctx, expr);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenExpr(ctx, expr);
        }
    } else if ( typeNode != NULL &&
                (xl6CTextEquals(typeNode->text, "int") || xl6CTextEquals(typeNode->text, "int8") ||
                 xl6CTextEquals(typeNode->text, "uint8") || xl6CTextEquals(typeNode->text, "int16") ||
                 xl6CTextEquals(typeNode->text, "uint16") || xl6CTextEquals(typeNode->text, "int32") ||
                 xl6CTextEquals(typeNode->text, "uint32") || xl6CTextEquals(typeNode->text, "int64") ||
                 xl6CTextEquals(typeNode->text, "uint64")) ) {
        xl6OutputPuts("xvoCreateInt((long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "point") ) {
        xl6OutputPuts("xvoCreatePoint((void*)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( typeNode != NULL &&
                (xl6CTextEquals(typeNode->text, "float") || xl6CTextEquals(typeNode->text, "float32") ||
                 xl6CTextEquals(typeNode->text, "float64")) ) {
        xl6OutputPuts("xvoCreateFloat((double)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "string") ) {
        xl6OutputPuts("xvoCreateText((void*)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("), 0, false)", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "time") ) {
        xl6OutputPuts("xvoCreateTime((long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsBoolTypeName(typeNode->text) ) {
        xl6OutputPuts("xvoCreateBool(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsFunctionType(typeNode) ) {
        xl6OutputPuts("xl6_function_to_xvalue(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsClassType(ctx, typeNode) ) {
        XL6_AstNode* ctorClass = NULL;
        int isCtor;
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPutc('(', ctx->out);
        isCtor = xl6CgenCallIsClassCtor(ctx, expr, &ctorClass);
        if ( !isCtor && expr->kind == XL6_AST_CALL && expr->childCount > 0 &&
             expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME &&
             xl6CTextEquals(expr->children[0]->text, typeNode->text) ) {
            ctorClass = xl6CgenFindClass(ctx, typeNode->text);
            isCtor = ctorClass != NULL;
        }
        if ( isCtor && ctorClass != NULL ) {
            const XL6_AstNode* args = expr->childCount > 1 ? expr->children[1] : NULL;
            if ( xl6CgenResolveClassCreate(ctx, ctorClass, expr) != NULL ||
                 (args != NULL && args->childCount > 0) ) {
                xl6CgenError(ctx, expr, "code generation error");
                xl6OutputPrintf(ctx->out, "(%s){0}", cTypeName);
            } else {
                xl6CgenDefaultClassSymbol(ctx->out, cTypeName);
                xl6OutputPuts("()", ctx->out);
            }
        } else {
            xl6CgenExpr(ctx, expr);
        }
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsUnionType(ctx, typeNode) ) {
        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), typeNode->text);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsCustomType(ctx, typeNode) ) {
        XL6_AstNode* ctorCustom = NULL;
        int isCtor;
        xl6CgenCustomTypeRefName(cTypeName, sizeof(cTypeName), typeNode->text);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPutc('(', ctx->out);
        isCtor = xl6CgenCallIsCustomCtor(ctx, expr, &ctorCustom);
        if ( !isCtor && expr->kind == XL6_AST_CALL && expr->childCount > 0 &&
             expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME &&
             xl6CTextEquals(expr->children[0]->text, typeNode->text) ) {
            ctorCustom = xl6CgenFindCustom(ctx, typeNode->text);
            isCtor = ctorCustom != NULL;
        }
        if ( isCtor && ctorCustom != NULL ) {
            xl6OutputPrintf(ctx->out, "(%s){", cTypeName);
            xl6CgenCustomCtorValue(ctx, ctorCustom, expr);
            xl6OutputPutc('}', ctx->out);
        } else {
            xl6CgenExpr(ctx, expr);
        }
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenError(ctx, expr, "当前 C 生成暂不支持将该表达式打包为 var");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
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
    int id;

    if ( !xl6CgenExprIsXValueResult(ctx, expr) ) {
        xl6CgenExpr(ctx, expr);
        return;
    }

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_truth_value_%d = ", id);
    xl6CgenXValueFromExpr(ctx, expr);
    xl6OutputPrintf(ctx->out,
                     "; int __xl6_truth_result_%d = xl6_xrt_value_truthy(__xl6_truth_value_%d); if (__xl6_truth_value_%d != 0) { xvoUnref(__xl6_truth_value_%d); } __xl6_truth_result_%d; })",
                     id,
                     id,
                     id,
                     id,
                     id);
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
    const XL6_AstNode* cond;
    const XL6_AstNode* thenExpr;
    const XL6_AstNode* elseExpr;
    int id;

    cond = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    thenExpr = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    elseExpr = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    id = ctx->tempId++;

    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_cond_result_%d; if (", id);
    xl6CgenConditionExpr(ctx, cond);
    xl6OutputPrintf(ctx->out, ") { __xl6_cond_result_%d = ", id);
    xl6CgenXValueFromExpr(ctx, thenExpr);
    xl6OutputPrintf(ctx->out, "; } else { __xl6_cond_result_%d = ", id);
    xl6CgenXValueFromExpr(ctx, elseExpr);
    xl6OutputPrintf(ctx->out, "; } __xl6_cond_result_%d; })", id);
}

static int xl6CgenBinaryCompareOpIsXValue(const char* op)
{
    return xl6CTextEquals(op, "==") || xl6CTextEquals(op, "!=") ||
           xl6CTextEquals(op, "<") || xl6CTextEquals(op, "<=") ||
           xl6CTextEquals(op, ">") || xl6CTextEquals(op, ">=");
}

static int xl6CgenBinaryCompareIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int result;

    if ( node == NULL || node->kind != XL6_AST_BINARY || node->childCount < 2 ||
         !xl6CgenBinaryCompareOpIsXValue(node->text) ) {
        return 0;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
    rightType = xl6CgenCloneExprTypeForInference(ctx, node->children[1]);
    result = xl6CgenIsXValueType(leftType) || xl6CgenIsXValueType(rightType);
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
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;
    int equalMode;
    int negate;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    id = ctx->tempId++;
    equalMode = node == NULL || xl6CTextEquals(node->text, "==") || xl6CTextEquals(node->text, "!=");
    negate = node != NULL && xl6CTextEquals(node->text, "!=");

    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_cmp_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPrintf(ctx->out, "; xvalue __xl6_cmp_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, right);
    if ( equalMode ) {
        xl6OutputPrintf(ctx->out,
                         "; int __xl6_cmp_result_%d = xl6_xrt_basic_equal(__xl6_cmp_left_%d, __xl6_cmp_right_%d); if (__xl6_cmp_left_%d != 0) { xvoUnref(__xl6_cmp_left_%d); } if (__xl6_cmp_right_%d != 0) { xvoUnref(__xl6_cmp_right_%d); } %s__xl6_cmp_result_%d; })",
                         id,
                         id,
                         id,
                         id,
                         id,
                         id,
                         id,
                         negate ? "!" : "",
                         id);
    } else {
        xl6OutputPrintf(ctx->out,
                         "; int __xl6_cmp_result_%d = xl6_xrt_basic_order(__xl6_cmp_left_%d, __xl6_cmp_right_%d, %d); if (__xl6_cmp_left_%d != 0) { xvoUnref(__xl6_cmp_left_%d); } if (__xl6_cmp_right_%d != 0) { xvoUnref(__xl6_cmp_right_%d); } __xl6_cmp_result_%d; })",
                         id,
                         id,
                         id,
                         xl6CgenXValueOrderMode(node != NULL ? node->text : "<"),
                         id,
                         id,
                         id,
                         id,
                         id);
    }
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
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    id = ctx->tempId++;

    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_approx_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPrintf(ctx->out, "; xvalue __xl6_approx_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPrintf(ctx->out,
                     "; int __xl6_approx_result_%d = xl6_xrt_basic_approx(__xl6_approx_left_%d, __xl6_approx_right_%d); if (__xl6_approx_left_%d != 0) { xvoUnref(__xl6_approx_left_%d); } if (__xl6_approx_right_%d != 0) { xvoUnref(__xl6_approx_right_%d); } __xl6_approx_result_%d; })",
                     id,
                     id,
                     id,
                     id,
                     id,
                     id,
                     id,
                     id);
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
    result = xl6CgenIsXValueType(leftType) || xl6CgenIsXValueType(rightType);
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
    int id;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    id = ctx->tempId++;

    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_bin_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPrintf(ctx->out, "; xvalue __xl6_bin_right_%d = ", id);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPrintf(ctx->out,
                     "; xvalue __xl6_bin_result_%d = xl6_xrt_basic_binary(__xl6_bin_left_%d, __xl6_bin_right_%d, %d); if (__xl6_bin_left_%d != 0) { xvoUnref(__xl6_bin_left_%d); } if (__xl6_bin_right_%d != 0) { xvoUnref(__xl6_bin_right_%d); } __xl6_bin_result_%d; })",
                     id,
                     id,
                     id,
                     xl6CgenXValueBinaryMode(node != NULL ? node->text : "+"),
                     id,
                     id,
                     id,
                     id,
                     id);
}
static int xl6CgenUnaryIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* valueType;
    int result;

    if ( node == NULL || node->kind != XL6_AST_UNARY || node->childCount == 0 ) {
        return 0;
    }

    valueType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
    result = xl6CgenIsXValueType(valueType);
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
    int id;

    value = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    id = ctx->tempId++;

    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_unary_value_%d = ", id);
    xl6CgenXValueFromExpr(ctx, value);
    xl6OutputPrintf(ctx->out,
                     "; xvalue __xl6_unary_result_%d = xl6_xrt_basic_unary(__xl6_unary_value_%d, %d); if (__xl6_unary_value_%d != 0) { xvoUnref(__xl6_unary_value_%d); } __xl6_unary_result_%d; })",
                     id,
                     id,
                     xl6CgenXValueUnaryMode(node != NULL ? node->text : "+"),
                     id,
                     id,
                     id);
}

static void xl6CgenXValueNotExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* value;
    int id;

    value = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    id = ctx->tempId++;

    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_not_value_%d = ", id);
    xl6CgenXValueFromExpr(ctx, value);
    xl6OutputPrintf(ctx->out,
                     "; int __xl6_not_result_%d = !xl6_xrt_value_truthy(__xl6_not_value_%d); if (__xl6_not_value_%d != 0) { xvoUnref(__xl6_not_value_%d); } __xl6_not_result_%d; })",
                     id,
                     id,
                     id,
                     id,
                     id);
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
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    int id;
    int isOr;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    id = ctx->tempId++;
    isOr = node != NULL && xl6CTextEquals(node->text, "or");

    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_logic_left_%d = ", id);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPrintf(ctx->out,
                     "; int __xl6_logic_left_truth_%d = xl6_xrt_value_truthy(__xl6_logic_left_%d); if (__xl6_logic_left_%d != 0) { xvoUnref(__xl6_logic_left_%d); } int __xl6_logic_result_%d = %s; if (%s__xl6_logic_left_truth_%d) { xvalue __xl6_logic_right_%d = ",
                     id,
                     id,
                     id,
                     id,
                     id,
                     isOr ? "1" : "0",
                     isOr ? "!" : "",
                     id,
                     id);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPrintf(ctx->out,
                     "; __xl6_logic_result_%d = xl6_xrt_value_truthy(__xl6_logic_right_%d); if (__xl6_logic_right_%d != 0) { xvoUnref(__xl6_logic_right_%d); } } __xl6_logic_result_%d; })",
                     id,
                     id,
                     id,
                     id,
                     id);
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
    int id;

    if ( xl6CgenExprIsXValueBorrow(ctx, expr) ) {
        xl6OutputPrintf(ctx->out, "((%s)%s(", cType != NULL ? cType : "int", getter != NULL ? getter : "xvoGetInt");
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
        return;
    }

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_conv_%d = ", id);
    xl6CgenExpr(ctx, expr);
    xl6OutputPrintf(ctx->out,
                     "; %s __xl6_conv_result_%d = (%s)(__xl6_conv_%d != 0 ? %s(__xl6_conv_%d) : 0); if (__xl6_conv_%d != 0) { xvoUnref(__xl6_conv_%d); } __xl6_conv_result_%d; })",
                     cType != NULL ? cType : "int",
                     id,
                     cType != NULL ? cType : "int",
                     id,
                     getter != NULL ? getter : "xvoGetInt",
                     id,
                     id,
                     id,
                     id);
}

static void xl6CgenTypeConvertExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* arg;
    XL6_AstNode* sourceType;
    const char* target;
    const char* source;
    int sourceIsXValue;

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
            if ( xl6CgenExprIsXValueBorrow(ctx, arg) ) {
                xl6OutputPuts("xl6_xrt_value_to_string(", ctx->out);
            } else {
                xl6OutputPuts("xl6_xrt_value_to_string_release(", ctx->out);
            }
            xl6CgenExpr(ctx, arg);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(target, "time") ) {
            xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetTime", arg);
        } else if ( xl6CTextEquals(target, "point") ) {
            xl6CgenXValueScalarGetExpr(ctx, xl6CgenConvertCTypeName(target), "xvoGetPoint", arg);
        } else {
            xl6CgenError(ctx, node, "当前 C 生成不支持该类型转换");
            xl6OutputPuts("0", ctx->out);
        }
    } else if ( xl6CgenIsIntegerTypeName(target) ) {
        xl6OutputPrintf(ctx->out, "((%s)(", xl6CgenConvertCTypeName(target));
        if ( xl6CTextEquals(source, "string") ) {
            xl6OutputPuts("xrtStrToI64(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(source, "null") ) {
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6CgenExpr(ctx, arg);
        }
        xl6OutputPuts("))", ctx->out);
    } else if ( xl6CgenIsFloatTypeName(target) ) {
        xl6OutputPrintf(ctx->out, "((%s)(", xl6CgenConvertCTypeName(target));
        if ( xl6CTextEquals(source, "string") ) {
            xl6OutputPuts("xrtStrToNum(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPutc(')', ctx->out);
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
            xl6OutputPuts("xl6_xrt_i64_to_string((long long)(", ctx->out);
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
        } else if ( xl6CTextEquals(source, "time") ) {
            xl6OutputPuts("xl6_xrt_take_cstr((char*)xrtTimeToStr((long long)(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("), 0))", ctx->out);
        } else if ( xl6CTextEquals(source, "point") ) {
            xl6OutputPuts("xl6_xrt_take_cstr((char*)xrtFormat(\"[point:%p]\", ", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("))", ctx->out);
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
        xl6CgenError(ctx, node, "当前 C 生成不支持该类型转换");
        xl6OutputPuts("0", ctx->out);
    }

    xl6AstFree(sourceType);
}

static void xl6CgenNullCoalesceExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    int isXValue;
    int id;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( left != NULL && left->kind == XL6_AST_NULL ) {
        xl6CgenExpr(ctx, right);
        return;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    isXValue = xl6CgenIsXValueType(leftType) || xl6CgenIsXValueType(rightType);

    if ( isXValue ) {
        if ( left == NULL || !xl6CgenIsXValueType(leftType) ) {
            xl6CgenError(ctx, node, "code generation error");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        } else {
            id = ctx->tempId++;
            xl6OutputPrintf(ctx->out, "({ xvalue __xl6_nc_%d = ", id);
            xl6CgenXValueFromExpr(ctx, left);
            xl6OutputPrintf(ctx->out, "; xvalue __xl6_nc_result_%d; if (xvoIsNull(__xl6_nc_%d)) { if (__xl6_nc_%d != 0) { xvoUnref(__xl6_nc_%d); } __xl6_nc_result_%d = ",
                             id,
                             id,
                             id,
                             id,
                             id);
            xl6CgenXValueFromExpr(ctx, right);
            xl6OutputPrintf(ctx->out, "; } else { __xl6_nc_result_%d = __xl6_nc_%d; } __xl6_nc_result_%d; })",
                             id,
                             id,
                             id);
        }
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    if ( leftType == NULL || rightType == NULL || !xl6CgenTypeNodeEqual(leftType, rightType) ) {
        xl6CgenError(ctx, node, "当前 C 生成�??? 两侧必须是同一静态类型，或左侧为 null/var");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    id = ctx->tempId++;
    xl6OutputPuts("({ ", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, leftType), ctx->out);
    xl6OutputPrintf(ctx->out, " __xl6_nc_%d = ", id);
    xl6CgenExpr(ctx, left);
    xl6OutputPrintf(ctx->out, "; __xl6_nc_%d != 0 ? __xl6_nc_%d : ", id, id);
    xl6CgenExpr(ctx, right);
    xl6OutputPuts("; })", ctx->out);

    xl6AstFree(leftType);
    xl6AstFree(rightType);
}

static void xl6CgenContainsXValueCallExpr(XL6_CgenContext* ctx, const char* funcName, const XL6_AstNode* values, const XL6_AstNode* item)
{
    int id;

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, values) ) {
        id = ctx->tempId++;
        xl6OutputPrintf(ctx->out, "({ xvalue __xl6_in_values_%d = ", id);
        xl6CgenXValueFromExpr(ctx, values);
        xl6OutputPrintf(ctx->out, "; int __xl6_in_result_%d = %s(__xl6_in_values_%d, ", id, funcName != NULL ? funcName : "xl6_xrt_value_contains", id);
        xl6CgenXValueFromExpr(ctx, item);
        xl6OutputPrintf(ctx->out, "); if (__xl6_in_values_%d != 0) { xvoUnref(__xl6_in_values_%d); } __xl6_in_result_%d; })", id, id, id);
        return;
    }

    if ( xl6CgenExprIsXValueBorrow(ctx, values) ) {
        xl6OutputPrintf(ctx->out, "%s(", funcName != NULL ? funcName : "xl6_xrt_value_contains");
        xl6CgenExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, item);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_in_values_%d = ", id);
    xl6CgenExpr(ctx, values);
    xl6OutputPrintf(ctx->out, "; int __xl6_in_result_%d = %s(__xl6_in_values_%d, ", id, funcName != NULL ? funcName : "xl6_xrt_value_contains", id);
    xl6CgenXValueFromExpr(ctx, item);
    xl6OutputPrintf(ctx->out, "); if (__xl6_in_values_%d != 0) { xvoUnref(__xl6_in_values_%d); } __xl6_in_result_%d; })", id, id, id);
}

static void xl6CgenDictContainsKeyExpr(XL6_CgenContext* ctx, const XL6_AstNode* values, const XL6_AstNode* key)
{
    int id;

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, values) ) {
        id = ctx->tempId++;
        xl6OutputPrintf(ctx->out, "({ xvalue __xl6_in_values_%d = ", id);
        xl6CgenXValueFromExpr(ctx, values);
        xl6OutputPrintf(ctx->out, "; int __xl6_in_result_%d = xvoTableExists(__xl6_in_values_%d, ", id, id);
        xl6CgenExpr(ctx, key);
        xl6OutputPrintf(ctx->out, ", 0); if (__xl6_in_values_%d != 0) { xvoUnref(__xl6_in_values_%d); } __xl6_in_result_%d; })", id, id, id);
        return;
    }

    if ( xl6CgenExprIsXValueBorrow(ctx, values) ) {
        xl6OutputPuts("xvoTableExists(", ctx->out);
        xl6CgenExpr(ctx, values);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, key);
        xl6OutputPuts(", 0)", ctx->out);
        return;
    }

    id = ctx->tempId++;
    xl6OutputPrintf(ctx->out, "({ xvalue __xl6_in_values_%d = ", id);
    xl6CgenExpr(ctx, values);
    xl6OutputPrintf(ctx->out, "; int __xl6_in_result_%d = xvoTableExists(__xl6_in_values_%d, ", id, id);
    xl6CgenExpr(ctx, key);
    xl6OutputPrintf(ctx->out, ", 0); if (__xl6_in_values_%d != 0) { xvoUnref(__xl6_in_values_%d); } __xl6_in_result_%d; })", id, id, id);
}
static void xl6CgenInExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* rightType;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( right != NULL && xl6CgenIsRangeCall(right) ) {
        xl6CgenRangeContainsExpr(ctx, left, right);
        return;
    }

    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    if ( rightType == NULL || rightType->text == NULL ) {
        xl6CgenError(ctx, node, "当前 C 生成 in 右侧类型必须可静态确定，或为 var");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "string") ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, left, "string") ) {
            xl6CgenError(ctx, node, "字符�?in 判断左侧必须�?string");
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6OutputPuts("(xrtFindStr(", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPuts(", 0, ", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", 0, false) != 0)", ctx->out);
        }
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CgenIsArrayXValueType(rightType) ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_array_contains", right, left);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "list") || xl6CTextEquals(rightType->text, "list<var>") ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_list_contains", right, left);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "set") || xl6CTextEquals(rightType->text, "set<var>") ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_set_contains", right, left);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CgenIsDictXValueType(rightType) ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, left, "string") ) {
            xl6CgenError(ctx, node, "code generation error");
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6CgenDictContainsKeyExpr(ctx, right, left);
        }
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "var") ) {
        xl6CgenContainsXValueCallExpr(ctx, "xl6_xrt_value_contains", right, left);
        xl6AstFree(rightType);
        return;
    }

    xl6CgenError(ctx, node, "当前 C 生成 in 右侧只支�?string、array、list、set、dict、range �?var");
    xl6OutputPuts("0", ctx->out);
    xl6AstFree(rightType);
}

static void xl6CgenArrayLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    xl6OutputPrintf(ctx->out, "xl6_xrt_array_varargs(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, node->children[i]);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenListLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    xl6OutputPrintf(ctx->out, "xl6_xrt_list_literal(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        const XL6_AstNode* pair = node->children[i];
        const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
        const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;

        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, key);
        xl6OutputPuts("), ", ctx->out);
        xl6CgenXValueFromExpr(ctx, value);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenSetLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    xl6OutputPrintf(ctx->out, "xl6_xrt_set_literal(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, node->children[i]);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenDictLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    size_t i;

    xl6OutputPrintf(ctx->out, "xl6_xrt_dict_varargs(%u", (unsigned)(node != NULL ? node->childCount : 0));
    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        const XL6_AstNode* pair = node->children[i];
        const XL6_AstNode* key = pair != NULL && pair->childCount > 0 ? pair->children[0] : NULL;
        const XL6_AstNode* value = pair != NULL && pair->childCount > 1 ? pair->children[1] : NULL;

        xl6OutputPuts(", ", ctx->out);
        if ( !xl6CgenExprStaticTypeIs(ctx, key, "string") ) {
            xl6CgenError(ctx, key, "当前 C 生成 dict 字面量键只支�?string");
            xl6OutputPuts("\"\"", ctx->out);
        } else {
            xl6CgenExpr(ctx, key);
        }
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, value);
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenTypedLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* literal = node != NULL && node->childCount > 0 ? node->children[0] : NULL;

    if ( xl6CTextEquals(node != NULL ? node->text : NULL, "array") ) {
        xl6CgenArrayLiteralExpr(ctx, literal);
    } else if ( xl6CTextEquals(node != NULL ? node->text : NULL, "list") ) {
        xl6CgenListLiteralExpr(ctx, literal);
    } else if ( xl6CTextEquals(node != NULL ? node->text : NULL, "set") ) {
        xl6CgenSetLiteralExpr(ctx, literal);
    } else if ( xl6CTextEquals(node != NULL ? node->text : NULL, "dict") ) {
        xl6CgenDictLiteralExpr(ctx, literal);
    } else {
        xl6CgenError(ctx, node, "code generation error");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
    }
}

static int xl6CgenArrayVarargCount(const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int positionalIndex;
    int count;

    positionalIndex = 0;
    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG ) {
            continue;
        }
        if ( positionalIndex >= fixedCount ) {
            count += 1;
        }
        positionalIndex += 1;
    }
    return count;
}

static void xl6CgenArrayVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int positionalIndex;
    int count;
    int emitted;

    count = xl6CgenArrayVarargCount(args, fixedCount);
    xl6OutputPrintf(ctx->out, "xl6_xrt_array_varargs(%d", count);
    positionalIndex = 0;
    emitted = 0;
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL || arg->kind == XL6_AST_NAMED_ARG ) {
                continue;
            }
            if ( positionalIndex >= fixedCount ) {
                xl6OutputPuts(", ", ctx->out);
                xl6CgenXValueFromExpr(ctx, xl6CgenArgExpr(arg));
                emitted += 1;
            }
            positionalIndex += 1;
        }
    }
    (void)emitted;
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenDictVarargCount(const XL6_AstNode* params, const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int count;

    count = 0;
    if ( args == NULL ) {
        return 0;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg != NULL && arg->kind == XL6_AST_NAMED_ARG &&
             xl6CgenFindParamIndex(params, fixedCount, arg->text) < 0 ) {
            count += 1;
        }
    }
    return count;
}

static void xl6CgenDictVarargExpr(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* args, int fixedCount)
{
    size_t i;
    int count;

    count = xl6CgenDictVarargCount(params, args, fixedCount);
    xl6OutputPrintf(ctx->out, "xl6_xrt_dict_varargs(%d", count);
    if ( args != NULL ) {
        for ( i = 0; i < args->childCount; ++i ) {
            const XL6_AstNode* arg = args->children[i];
            if ( arg == NULL || arg->kind != XL6_AST_NAMED_ARG ||
                 xl6CgenFindParamIndex(params, fixedCount, arg->text) >= 0 ) {
                continue;
            }
            xl6OutputPuts(", ", ctx->out);
            xl6CgenCStringLiteral(ctx->out, arg->text);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenXValueFromExpr(ctx, xl6CgenArgExpr(arg));
        }
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenKnownCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* args)
{
    const XL6_AstNode* params;
    int fixedCount;
    int hasVararg;
    int i;

    params = func->childCount > 1 ? func->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(params);
    hasVararg = params != NULL && params->childCount > (size_t)fixedCount;

    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* expr = xl6CgenFindArgForParam(args, param->text, i);

        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( expr != NULL ) {
            xl6CgenExpr(ctx, expr);
        } else if ( param->childCount > 1 ) {
            xl6CgenExpr(ctx, param->children[1]);
        } else {
            xl6CgenError(ctx, param, "code generation error");
            xl6OutputPuts("0", ctx->out);
        }
    }
    if ( hasVararg ) {
        const XL6_AstNode* vararg = params->children[fixedCount];
        if ( fixedCount > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( xl6CgenIsArrayXValueType(vararg->children[0]) ) {
            xl6CgenArrayVarargExpr(ctx, args, fixedCount);
        } else if ( xl6CgenIsDictXValueType(vararg->children[0]) ) {
            xl6CgenDictVarargExpr(ctx, params, args, fixedCount);
        } else {
            xl6CgenError(ctx, vararg, "未知可变参数类型");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
    }
}

static void xl6CgenParamNodeArgs(XL6_CgenContext* ctx, const XL6_AstNode* params, const XL6_AstNode* args)
{
    int fixedCount;
    int i;

    fixedCount = xl6CgenFixedParamCount(params);
    if ( params != NULL && params->childCount > (size_t)fixedCount ) {
        xl6CgenError(ctx, params, "当前 C 生成暂不支持可变参数调用");
    }
    for ( i = 0; i < fixedCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* expr = xl6CgenFindArgForParam(args, param->text, i);
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        if ( expr != NULL ) {
            xl6CgenExpr(ctx, expr);
        } else if ( param->childCount > 1 ) {
            xl6CgenExpr(ctx, param->children[1]);
        } else {
            xl6CgenError(ctx, param, "code generation error");
            xl6OutputPuts("0", ctx->out);
        }
    }
}
