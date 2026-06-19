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
    if ( xl6CgenExprUsesTypedContainerStorage(ctx, expr) ) {
        xl6CgenError(ctx, expr, "静态泛型容器暂不能直接装箱�?var");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
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
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "bool") ) {
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
        if ( xl6CgenIsIntegerTypeName(target) ) {
            xl6OutputPrintf(ctx->out, "((%s)xvoGetInt(", xl6CgenConvertCTypeName(target));
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CgenIsFloatTypeName(target) ) {
            xl6OutputPrintf(ctx->out, "((%s)xvoGetFloat(", xl6CgenConvertCTypeName(target));
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("))", ctx->out);
        } else if ( xl6CTextEquals(target, "bool") ) {
            xl6OutputPuts("xvoGetBool(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(target, "string") ) {
            xl6OutputPuts("xl6_xrt_value_to_string(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(target, "time") ) {
            xl6OutputPuts("xvoGetTime(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPutc(')', ctx->out);
        } else if ( xl6CTextEquals(target, "point") ) {
            xl6OutputPuts("xvoGetPoint(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPutc(')', ctx->out);
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
    } else if ( xl6CTextEquals(target, "bool") ) {
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
        } else if ( xl6CTextEquals(source, "bool") ) {
            xl6OutputPutc('(', ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts(" ? \"true\" : \"false\")", ctx->out);
        } else if ( xl6CTextEquals(source, "time") ) {
            xl6OutputPuts("xrtTimeToStr((long long)(", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPuts("), 0)", ctx->out);
        } else if ( xl6CTextEquals(source, "point") ) {
            xl6OutputPuts("xrtFormat(\"[point:%p]\", ", ctx->out);
            xl6CgenExpr(ctx, arg);
            xl6OutputPutc(')', ctx->out);
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
            xl6OutputPuts("(xvoIsNull(", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(") ? ", ctx->out);
            xl6CgenXValueFromExpr(ctx, right);
            xl6OutputPuts(" : ", ctx->out);
            xl6CgenXValueFromExpr(ctx, left);
            xl6OutputPutc(')', ctx->out);
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

    xl6OutputPutc('(', ctx->out);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts(" != 0 ? ", ctx->out);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts(" : ", ctx->out);
    xl6CgenExpr(ctx, right);
    xl6OutputPutc(')', ctx->out);

    xl6AstFree(leftType);
    xl6AstFree(rightType);
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
        xl6OutputPuts("xl6_xrt_array_contains(", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, left);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "list") || xl6CTextEquals(rightType->text, "list<var>") ) {
        xl6OutputPuts("xl6_xrt_list_contains(", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, left);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "set") || xl6CTextEquals(rightType->text, "set<var>") ) {
        xl6OutputPuts("xl6_xrt_set_contains(", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, left);
        xl6OutputPutc(')', ctx->out);
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CgenIsDictXValueType(rightType) ) {
        if ( !xl6CgenExprStaticTypeIs(ctx, left, "string") ) {
            xl6CgenError(ctx, node, "code generation error");
            xl6OutputPuts("0", ctx->out);
        } else {
            xl6OutputPuts("xvoTableExists(", ctx->out);
            xl6CgenExpr(ctx, right);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, left);
            xl6OutputPuts(", 0)", ctx->out);
        }
        xl6AstFree(rightType);
        return;
    }

    if ( xl6CTextEquals(rightType->text, "var") ) {
        xl6OutputPuts("xl6_xrt_value_contains(", ctx->out);
        xl6CgenExpr(ctx, right);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenXValueFromExpr(ctx, left);
        xl6OutputPutc(')', ctx->out);
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
