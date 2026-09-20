static void xl6CgenKnownCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* args);
static void xl6CgenKnownCAbiCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* args);
static int xl6CgenNativeClassCtorHasPointBindingArgs(const XL6_AstNode* args);
static void xl6CgenNativeBackedPointBindingValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* argExpr);
static void xl6CgenArrayLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenListLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenSetLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenDictLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);
static void xl6CgenTypedLiteralExpr(XL6_CgenContext* ctx, const XL6_AstNode* node);

static int xl6CgenParamIsVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           (xl6CTextEquals(param->children[0]->text, "array<var>") ||
            xl6CTextEquals(param->children[0]->text, "dict<var>") ||
            xl6CTextEquals(param->children[0]->text, "c_vararg"));
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

static int xl6CgenParamIsCVararg(const XL6_AstNode* param)
{
    return param != NULL && param->childCount > 0 &&
           param->children[0] != NULL &&
           xl6CTextEquals(param->children[0]->text, "c_vararg");
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

static int xl6CgenTryNativeClassCtorBoxValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* call,
    const char* cTypeName)
{
    const XL6_AstNode* createNode;
    const XL6_AstNode* args;

    if ( ctx == NULL || classNode == NULL || call == NULL || cTypeName == NULL ||
         !xl6CgenClassIsPureNativeHandle(classNode) ) {
        return 0;
    }

    args = call->childCount > 1 ? call->children[1] : NULL;
    createNode = xl6CgenResolveClassCreateMode(ctx, classNode, call, 0);
    if ( createNode != NULL ) {
        if ( createNode->kind != XL6_AST_NATIVE_EXTERN_SPECIAL ) {
            return 0;
        }
        xl6OutputPrintf(ctx->out, "(%s){", cTypeName);
        xl6CgenCStringSymbol(ctx->out, createNode->text);
        xl6OutputPutc('(', ctx->out);
        xl6CgenKnownCAbiCallArgs(ctx, createNode, args);
        xl6OutputPuts(")}", ctx->out);
        return 1;
    }

    if ( xl6CgenNativeClassCtorHasPointBindingArgs(args) ) {
        const XL6_AstNode* arg = args->children[0];
        xl6OutputPrintf(ctx->out, "(%s){", cTypeName);
        xl6CgenNativeBackedPointBindingValue(ctx, classNode, arg->children[0]);
        xl6OutputPutc('}', ctx->out);
        return 1;
    }

    if ( args == NULL || args->childCount == 0 ) {
        xl6OutputPrintf(ctx->out, "(%s){0}", cTypeName);
        return 1;
    }

    return 0;
}

static int xl6CgenTryClassCtorBoxXValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* call,
    const char* cTypeName)
{
    if ( ctx == NULL || classNode == NULL || call == NULL || cTypeName == NULL ||
         call->kind != XL6_AST_CALL ||
         xl6CgenClassIsPureNativeHandle(classNode) ||
         xl6CgenClassHasTypeParams(classNode) ) {
        return 0;
    }

    /*
        构造、装箱和析构必须由语句级路径完成。
        到达这里说明调用方漏掉了临时量 lowering，直接报代码生成错误，
        不再用 GNU statement expression 隐式兜底。
    */
    xl6CgenError(ctx, call, "class constructor boxing requires statement-level lowering");
    xl6OutputPuts("xrtValueNull()", ctx->out);
    return 1;
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
        if ( ch == '\r' ) {
            if ( i + 1 < end && text[i + 1] == '\n' ) {
                ++i;
            }
            xl6CgenWriteCStringChar(ctx->out, '\n');
            continue;
        }
        xl6CgenWriteCStringChar(ctx->out, ch);
    }
    xl6OutputPutc('"', ctx->out);
}

static void xl6CgenFutureToXValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr != NULL && expr->kind == XL6_AST_CONDITIONAL && expr->childCount > 2 ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenConditionExpr(ctx, expr->children[0]);
        xl6OutputPuts(" ? ", ctx->out);
        xl6CgenFutureToXValueExpr(ctx, expr->children[1]);
        xl6OutputPuts(" : ", ctx->out);
        xl6CgenFutureToXValueExpr(ctx, expr->children[2]);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6OutputPuts("xl6_future_to_xvalue(", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPrintf(ctx->out, ", %d)", expr != NULL && expr->kind == XL6_AST_CALL ? 1 : 0);
}

static void xl6CgenXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawTypeNode;
    XL6_AstNode* storageTypeNode;
    const XL6_AstNode* typeNode;
    XL6_AstNode* bareFunction;
    char cTypeName[512];
    int bareFunctionCount;
    int hasReplacement;
    unsigned int replacementFlags;
    size_t replacementIndex;

    if ( expr == NULL ) {
        xl6OutputPuts("xrtValueNull()", ctx->out);
        return;
    }
    if ( expr->kind == XL6_AST_NULL ) {
        xl6OutputPuts("xrtValueNull()", ctx->out);
        return;
    }

    bareFunction = NULL;
    bareFunctionCount = 0;
    if ( expr->kind == XL6_AST_NAME && xl6CgenFindVarType(ctx, expr->text) == NULL ) {
        bareFunction = xl6CgenFindUniqueBareFunctionValueFunc(ctx, expr->text, &bareFunctionCount);
        if ( bareFunction != NULL ) {
            xl6OutputPuts("xl6_function_to_xvalue(", ctx->out);
            xl6CgenFunctionValueMakeCall(ctx, bareFunction, NULL);
            xl6OutputPutc(')', ctx->out);
            return;
        }
        if ( bareFunctionCount > 1 ) {
            xl6CgenError(ctx, expr, "bare overloaded function assigned to var requires an explicit func signature");
            xl6OutputPuts("xrtValueNull()", ctx->out);
            return;
        }
    }
    hasReplacement = 0;
    replacementFlags = 0u;
    for ( replacementIndex = 0; replacementIndex < ctx->exprReplacementCount; ++replacementIndex ) {
        if ( ctx->exprReplacements[replacementIndex].expr == expr &&
             ctx->exprReplacements[replacementIndex].cName != NULL ) {
            hasReplacement = 1;
            replacementFlags = ctx->exprReplacements[replacementIndex].flags;
            break;
        }
    }

    /*
        普通容器字面量写入 var 时固定生成动态 xvalue* 容器。
        array<int> 这类显式静态泛型容器仍由 typed-container 路径处理。
    */
    if ( expr->kind == XL6_AST_ARRAY_LITERAL ) {
        xl6CgenArrayLiteralExpr(ctx, expr);
        return;
    }
    if ( expr->kind == XL6_AST_LIST_LITERAL ) {
        xl6CgenListLiteralExpr(ctx, expr);
        return;
    }
    if ( expr->kind == XL6_AST_SET_LITERAL ) {
        xl6CgenSetLiteralExpr(ctx, expr);
        return;
    }
    if ( expr->kind == XL6_AST_DICT_LITERAL ) {
        xl6CgenDictLiteralExpr(ctx, expr);
        return;
    }
    if ( expr->kind == XL6_AST_TYPED_LITERAL &&
         (expr->childCount <= 1 || expr->children[1] == NULL) &&
         (xl6CTextEquals(expr->text, "array") || xl6CTextEquals(expr->text, "list") ||
          xl6CTextEquals(expr->text, "set") || xl6CTextEquals(expr->text, "dict")) ) {
        xl6CgenTypedLiteralExpr(ctx, expr);
        return;
    }

    if ( xl6CgenTypedContainerXValueFromExpr(ctx, expr) ) {
        return;
    }
    if ( hasReplacement && (replacementFlags & XL6_CGEN_EXPR_REPLACEMENT_XVALUE) != 0 ) {
        xl6OutputPuts("xl6_xrt_retain(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    /*
        var 参与的算术表达式本身已经生成 xvalue*。
        其语言级推断结果可能因另一侧为 string 而收窄，不能再次按静态值装箱。
    */
    if ( xl6CgenBinaryArithmeticIsXValue(ctx, expr) ) {
        xl6CgenExpr(ctx, expr);
        return;
    }

    /*
        流分析可以把 var 名字收窄成具体类型，但它在 C 中仍然保存为 xvalue*。
        向 var 返回值或实参传递时必须读取真实存储，不能按收窄类型再次装箱。
    */
    storageTypeNode = xl6CgenCloneExprStorageType(ctx, expr);
    if ( expr->kind == XL6_AST_NAME && xl6CgenIsXValueType(storageTypeNode) ) {
        if ( xl6CgenExprIsXValueBorrow(ctx, expr) ) {
            xl6OutputPuts("xl6_xrt_retain(", ctx->out);
            xl6CgenNamedValueReference(ctx, expr->text);
            xl6OutputPutc(')', ctx->out);
        } else {
            xl6CgenNamedValueReference(ctx, expr->text);
        }
        xl6AstFree(storageTypeNode);
        return;
    }
    xl6AstFree(storageTypeNode);

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
                xl6CgenNullableUsesTaggedValue(typeNode) ) {
        char boxName[64];

        xl6CgenNullableBoxHelperName(boxName, sizeof(boxName), typeNode);
        xl6OutputPrintf(ctx->out, "%s(", boxName);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsNullableType(typeNode) ) {
        /*
            typed literal 先通过 xvalue* 交换元素。pointer/xvalue* nullable 也必须
            使用自己的 TypeDesc 装箱，不能把 T? 当作 T 或退回逐类型分支。
        */
        xl6OutputPuts("xl6TypeBoxValue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeNode);
        xl6OutputPuts(", &(", ctx->out);
        xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
        xl6OutputPuts("){", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("})", ctx->out);
    } else if ( typeNode != NULL &&
                xl6CgenIsTaggedEnumType(ctx, typeNode) ) {
        xl6CgenEnumOpsSymbol(ctx->out, typeNode, "box_value");
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL &&
                xl6CgenIsNumericEnumType(ctx, typeNode) ) {
        xl6CgenEnumOpsSymbol(ctx->out, typeNode, "box_value");
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsIntegerTypeName(typeNode->text) ) {
        xl6OutputPuts("xrtValueInt((long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "point") ) {
        xl6OutputPuts("xrtValuePointer((void*)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "type") ) {
        xl6OutputPuts("xl6_xrt_value_text_create((void*)xl6_program_type_name_from_id((unsigned long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(")), 0, false)", ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsFloatTypeName(typeNode->text) ) {
        xl6OutputPuts("xrtValueFloat((double)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "string") ) {
        xl6OutputPuts("xl6_xrt_value_text_create((void*)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("), 0, false)", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "time") ) {
        xl6OutputPuts("xrtValueTime((long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("))", ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsBoolTypeName(typeNode->text) ) {
        xl6OutputPuts("xrtValueBool(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsFunctionType(typeNode) ) {
        xl6OutputPuts("xl6_function_to_xvalue(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsFutureLikeType(typeNode) ) {
        xl6CgenFutureToXValueExpr(ctx, expr);
    } else if ( typeNode != NULL && xl6CgenIsSpecialCollectionType(ctx, typeNode) ) {
        xl6OutputPuts("xl6TypeBoxValue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, typeNode);
        xl6OutputPuts(", &(", ctx->out);
        xl6OutputPuts(xl6CgenTypeName(ctx, typeNode), ctx->out);
        xl6OutputPuts("){", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("})", ctx->out);
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "error") ) {
        xl6CgenErrorBoxExpr(ctx, expr);
    } else if ( typeNode != NULL && xl6CgenIsClassType(ctx, typeNode) ) {
        XL6_AstNode* ctorClass = NULL;
        int isCtor;
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), typeNode);
        isCtor = !hasReplacement && xl6CgenCallIsClassCtor(ctx, expr, &ctorClass);
        if ( !hasReplacement && !isCtor && expr->kind == XL6_AST_CALL && expr->childCount > 0 &&
             expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME &&
             xl6CTextEquals(expr->children[0]->text, typeNode->text) ) {
            ctorClass = xl6CgenFindClass(ctx, typeNode->text);
            isCtor = ctorClass != NULL;
        }
        if ( isCtor && ctorClass != NULL &&
             xl6CgenTryClassCtorBoxXValue(ctx, ctorClass, expr, cTypeName) ) {
            ;
        } else {
            xl6CgenBoxSymbol(ctx->out, cTypeName);
            xl6OutputPutc('(', ctx->out);
            if ( xl6CgenTryNativeClassCtorBoxValue(ctx, ctorClass, expr, cTypeName) ) {
                /* 纯 native handle class 保持直接装箱能力。 */
            } else if ( isCtor && ctorClass != NULL ) {
                xl6CgenDefaultClassSymbol(ctx->out, cTypeName);
                xl6OutputPuts("()", ctx->out);
            } else {
                xl6CgenExpr(ctx, expr);
            }
            xl6OutputPutc(')', ctx->out);
        }
    } else if ( typeNode != NULL && xl6CgenIsPlainRecordType(ctx, typeNode) ) {
        xl6CgenUnionTypeTextName(cTypeName, sizeof(cTypeName), typeNode->text);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( typeNode != NULL && xl6CgenIsNativeBackedClassType(ctx, typeNode) ) {
        XL6_AstNode* ctorNativeClass = NULL;
        int isCtor;
        xl6CgenNativeBackedTypeRefName(cTypeName, sizeof(cTypeName), typeNode->text);
        xl6CgenBoxSymbol(ctx->out, cTypeName);
        xl6OutputPutc('(', ctx->out);
        isCtor = !hasReplacement && xl6CgenCallIsNativeBackedCtor(ctx, expr, &ctorNativeClass);
        if ( !hasReplacement && !isCtor && expr->kind == XL6_AST_CALL && expr->childCount > 0 &&
             expr->children[0] != NULL && expr->children[0]->kind == XL6_AST_NAME &&
             xl6CTextEquals(expr->children[0]->text, typeNode->text) ) {
            ctorNativeClass = xl6CgenFindNativeBackedClass(ctx, typeNode->text);
            isCtor = ctorNativeClass != NULL;
        }
        if ( isCtor && ctorNativeClass != NULL ) {
            xl6OutputPrintf(ctx->out, "(%s){", cTypeName);
            xl6CgenNativeBackedCtorValue(ctx, ctorNativeClass, expr);
            xl6OutputPutc('}', ctx->out);
        } else {
            xl6CgenExpr(ctx, expr);
        }
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenError(ctx, expr, "当前 C 生成暂不支持将该表达式打包为 var");
        xl6OutputPuts("xrtValueNull()", ctx->out);
    }
    xl6AstFree(rawTypeNode);
}

