static void xl6CgenKnownCallArgs(XL6_CgenContext* ctx, const XL6_AstNode* func, const XL6_AstNode* args);
static int xl6CgenNativeClassCtorHasPointBindingArgs(const XL6_AstNode* args);
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
        xl6CgenKnownCallArgs(ctx, createNode, args);
        xl6OutputPuts(")}", ctx->out);
        return 1;
    }

    if ( xl6CgenNativeClassCtorHasPointBindingArgs(args) ) {
        const XL6_AstNode* arg = args->children[0];
        xl6OutputPrintf(ctx->out, "(%s){", cTypeName);
        xl6CgenExpr(ctx, arg->children[0]);
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
    xl6OutputPuts("xvoCreateNull()", ctx->out);
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

static int xl6CgenTypedContainerXValueFromExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    XL6_AstNode* rawTypeNode;
    const XL6_AstNode* typeNode;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    XL6_AstNode* itemClass;
    const char* typeName;
    int usesStorage;
    int returnCall;

    usesStorage = xl6CgenExprUsesTypedContainerStorage(ctx, expr);
    returnCall = !usesStorage && xl6CgenExprIsTypedContainerReturnCall(ctx, expr);
    if ( !usesStorage && !returnCall ) {
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

    if ( returnCall ) {
        xl6CgenError(ctx, expr, "typed container return boxing requires statement-level lowering");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }

    xl6OutputPuts("xl6_xrt_bind_type(", ctx->out);
    itemClass = itemType != NULL && itemType->text != NULL ? xl6CgenFindClass(ctx, itemType->text) : NULL;
    if ( xl6CTextEquals(typeName, "array") &&
         itemClass != NULL &&
         xl6CgenClassHasValueDerive(itemClass) ) {
        xl6OutputPuts("xl6_xrt_typed_array_to_xvalue_map(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(", (xl6_xrt_typed_item_box_fn)", ctx->out);
        xl6CgenClassAutoToValueFormatSymbol(ctx->out, itemClass, NULL);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "array") ) {
        xl6OutputPuts("xl6_xrt_typed_array_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "list") ) {
        xl6OutputPuts("xl6_xrt_typed_list_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "set") ) {
        xl6OutputPuts("xl6_xrt_typed_set_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else if ( xl6CTextEquals(typeName, "dict") ) {
        xl6OutputPuts("xl6_xrt_typed_dict_to_xvalue(", ctx->out);
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenError(ctx, expr, "typed container boxing requires array/list/set/dict");
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        xl6AstFree(rawTypeNode);
        return 1;
    }
    xl6OutputPuts(", ", ctx->out);
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
    int hasReplacement;
    size_t replacementIndex;

    if ( expr == NULL ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }
    if ( expr->kind == XL6_AST_NULL ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }
    hasReplacement = 0;
    for ( replacementIndex = 0; replacementIndex < ctx->exprReplacementCount; ++replacementIndex ) {
        if ( ctx->exprReplacements[replacementIndex].expr == expr &&
             ctx->exprReplacements[replacementIndex].cName != NULL ) {
            hasReplacement = 1;
            break;
        }
    }

    /*
        普通容器字面量写入 var 时固定生成动态 xvalue 容器。
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
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "type") ) {
        xl6OutputPuts("xvoCreateText((void*)xl6_program_type_name_from_id((unsigned long long)(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts(")), 0, false)", ctx->out);
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
    } else if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "error") ) {
        xl6OutputPuts("xvoCreateText((void*)xl6_error_message(", ctx->out);
        xl6CgenExpr(ctx, expr);
        xl6OutputPuts("), 0, false)", ctx->out);
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
    } else if ( typeNode != NULL && xl6CgenIsUnionType(ctx, typeNode) ) {
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
    return xl6CgenIsXValueType(typeNode) && xl6CgenRefTargetType(ctx, typeNode) == NULL;
}

static int xl6CgenBinaryCompareHasRef(XL6_CgenContext* ctx, const XL6_AstNode* node)
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
    result = xl6CgenRefTargetType(ctx, leftType) != NULL ||
             xl6CgenRefTargetType(ctx, rightType) != NULL;
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
    result = xl6CgenBinaryCompareHasRef(ctx, node) ||
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
            xl6CgenBinaryCompareHasRef(ctx, node) ?
                "xl6_ref_identity_equal_release(" :
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

    if ( node == NULL || node->childCount < 2 ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }

    left = node->children[0];
    right = node->children[1];
    xl6OutputPuts("xl6_xrt_basic_binary_release(", ctx->out);
    xl6CgenXValueFromExpr(ctx, left);
    xl6OutputPuts(", ", ctx->out);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPrintf(ctx->out, ", %d)", xl6CgenXValueBinaryMode(node->text));
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

static int xl6CgenClassToStringConversionExpr(XL6_CgenContext* ctx, const XL6_AstNode* arg, const XL6_AstNode* sourceType)
{
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    const XL6_AstNode* method;

    concreteType = xl6CgenResolveAliasType(ctx, sourceType);
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
            xl6CgenError(ctx, node, "null coalesce with var result requires the left operand to be var or null");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        } else {
            xl6CgenError(ctx, node, "dynamic var null coalesce requires statement-level lowering");
            xl6OutputPuts("xvoCreateNull()", ctx->out);
        }
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    if ( leftType == NULL || rightType == NULL || !xl6CgenTypeNodeEqual(leftType, rightType) ) {
        xl6CgenError(ctx, node, "当前 C 生成 ?? 两侧必须是同一静态类型，或左侧为 null/var");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    xl6CgenError(ctx, node, "static null coalesce requires statement-level lowering");
    xl6OutputPuts("0", ctx->out);

    xl6AstFree(leftType);
    xl6AstFree(rightType);
}
