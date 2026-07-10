static void xl6CgenClassPersistenceFromValueHelperSymbol(XL6_Output* out, const XL6_AstNode* classNode)
{
    xl6CgenClassMemberSymbol(out, classNode != NULL ? classNode->text : "_", "__xl6_from_value_release");
}

static void xl6CgenClassPersistenceFormatHelperSymbol(
    XL6_Output* out,
    const XL6_AstNode* classNode,
    const char* format,
    int isWrite)
{
    (void)format;
    xl6CgenClassMemberSymbol(out, classNode != NULL ? classNode->text : "_",
                             isWrite ? "__xl6_write_value" : "__xl6_stringify_value");
}

static void xl6CgenClassPersistenceFromValueHelperProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6OutputPuts("static ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenClassPersistenceFromValueHelperSymbol(ctx->out, classNode);
    xl6OutputPuts("(xvalue value, int formatKind)", ctx->out);
}

static void xl6CgenClassPersistenceStringifyHelperProto(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* format)
{
    (void)format;
    xl6OutputPuts("static const char* ", ctx->out);
    xl6CgenClassPersistenceFormatHelperSymbol(ctx->out, classNode, NULL, 0);
    xl6OutputPutc('(', ctx->out);
    xl6OutputPuts("const char* (*stringify_fn)(xvalue, int), ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(" source, int borrowed, int pretty, int formatKind)", ctx->out);
}

static void xl6CgenClassPersistenceWriteHelperProto(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* format)
{
    (void)format;
    xl6OutputPuts("static int ", ctx->out);
    xl6CgenClassPersistenceFormatHelperSymbol(ctx->out, classNode, NULL, 1);
    xl6OutputPuts("(int (*write_fn)(const char*, xvalue, int), const char* path, ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(" source, int borrowed, int pretty, int formatKind)", ctx->out);
}

static int xl6CgenPersistenceFormatKind(const char* format)
{
    if ( xl6CTextEquals(format, "json") ) {
        return 1;
    }
    if ( xl6CTextEquals(format, "xson") ) {
        return 2;
    }
    return 0;
}

static void xl6CgenClassPersistenceHelperProtos(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    if ( ctx == NULL || classNode == NULL || !xl6CgenClassHasValueDerive(classNode) ) {
        return;
    }
    xl6CgenClassPersistenceFromValueHelperProto(ctx, classNode);
    xl6OutputPuts(";\n", ctx->out);
    if ( xl6CgenClassHasFormatDerive(classNode, "json") ||
         xl6CgenClassHasFormatDerive(classNode, "xson") ) {
        xl6CgenClassPersistenceStringifyHelperProto(ctx, classNode, "json");
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenClassPersistenceWriteHelperProto(ctx, classNode, "json");
        xl6OutputPuts(";\n", ctx->out);
    }
}

static void xl6CgenClassPersistenceFromValueHelper(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    int hasJson;
    int hasXson;
    int emittedBranch;

    xl6CgenClassPersistenceFromValueHelperProto(ctx, classNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(" object = {0};\n", ctx->out);
    xl6CgenClassDefaultInitFields(ctx, classNode, "object");
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( value != 0 ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__fromValue__") ) {
        hasJson = xl6CgenClassHasFormatDerive(classNode, "json");
        hasXson = xl6CgenClassHasFormatDerive(classNode, "xson");
        emittedBranch = 0;
        if ( hasJson ) {
            xl6OutputPuts("if ( formatKind == 1 ) { ", ctx->out);
            xl6CgenClassAutoFromValueFormatSymbol(ctx->out, classNode, "json");
            xl6OutputPuts("(&object, value); }\n", ctx->out);
            emittedBranch = 1;
        }
        if ( hasXson ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts(emittedBranch ? "else if ( formatKind == 2 ) { " : "if ( formatKind == 2 ) { ", ctx->out);
            xl6CgenClassAutoFromValueFormatSymbol(ctx->out, classNode, "xson");
            xl6OutputPuts("(&object, value); }\n", ctx->out);
            emittedBranch = 1;
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts(emittedBranch ? "else { " : "", ctx->out);
        xl6CgenClassAutoFromValueFormatSymbol(ctx->out, classNode, NULL);
        xl6OutputPuts(emittedBranch ? "(&object, value); }\n" : "(&object, value);\n", ctx->out);
    } else {
        xl6CgenEmitClassFromValueSymbol(ctx, classNode);
        xl6OutputPuts("(&object, value);\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvoUnref(value);\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("return object;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassPersistenceStringifyHelper(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* format)
{
    int hasJson;
    int hasXson;
    int emittedBranch;

    (void)format;
    xl6CgenClassPersistenceStringifyHelperProto(ctx, classNode, format);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue value;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("const char* text;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( borrowed ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenRetainClassFunctionFields(ctx, classNode, "source");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__toValue__") ) {
        hasJson = xl6CgenClassHasFormatDerive(classNode, "json");
        hasXson = xl6CgenClassHasFormatDerive(classNode, "xson");
        emittedBranch = 0;
        if ( hasJson ) {
            xl6OutputPuts("if ( formatKind == 1 ) { value = ", ctx->out);
            xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, "json");
            xl6OutputPuts("(&source); }\n", ctx->out);
            emittedBranch = 1;
        }
        if ( hasXson ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts(emittedBranch ? "else if ( formatKind == 2 ) { value = " : "if ( formatKind == 2 ) { value = ", ctx->out);
            xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, "xson");
            xl6OutputPuts("(&source); }\n", ctx->out);
            emittedBranch = 1;
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts(emittedBranch ? "else { value = " : "value = ", ctx->out);
        xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, NULL);
        xl6OutputPuts(emittedBranch ? "(&source); }\n" : "(&source);\n", ctx->out);
    } else {
        xl6OutputPuts("value = ", ctx->out);
        xl6CgenEmitClassToValueSymbol(ctx, classNode);
        xl6OutputPuts("(&source);\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("text = stringify_fn != 0 ? stringify_fn(value, pretty ? 1 : 0) : \"\";\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( value != 0 ) { xvoUnref(value); }\n", ctx->out);
    if ( xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        xl6OutputPuts("(&source);\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("return text;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassPersistenceWriteHelper(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* format)
{
    int hasJson;
    int hasXson;
    int emittedBranch;

    (void)format;
    xl6CgenClassPersistenceWriteHelperProto(ctx, classNode, format);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue value;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("int ok;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( borrowed ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenRetainClassFunctionFields(ctx, classNode, "source");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__toValue__") ) {
        hasJson = xl6CgenClassHasFormatDerive(classNode, "json");
        hasXson = xl6CgenClassHasFormatDerive(classNode, "xson");
        emittedBranch = 0;
        if ( hasJson ) {
            xl6OutputPuts("if ( formatKind == 1 ) { value = ", ctx->out);
            xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, "json");
            xl6OutputPuts("(&source); }\n", ctx->out);
            emittedBranch = 1;
        }
        if ( hasXson ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts(emittedBranch ? "else if ( formatKind == 2 ) { value = " : "if ( formatKind == 2 ) { value = ", ctx->out);
            xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, "xson");
            xl6OutputPuts("(&source); }\n", ctx->out);
            emittedBranch = 1;
        }
        xl6CgenIndent(ctx);
        xl6OutputPuts(emittedBranch ? "else { value = " : "value = ", ctx->out);
        xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, NULL);
        xl6OutputPuts(emittedBranch ? "(&source); }\n" : "(&source);\n", ctx->out);
    } else {
        xl6OutputPuts("value = ", ctx->out);
        xl6CgenEmitClassToValueSymbol(ctx, classNode);
        xl6OutputPuts("(&source);\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("ok = write_fn != 0 ? (write_fn(path != 0 ? path : \"\", value, pretty ? 1 : 0) ? 1 : 0) : 0;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( value != 0 ) { xvoUnref(value); }\n", ctx->out);
    if ( xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        xl6OutputPuts("(&source);\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("return ok;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassPersistenceHelpers(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    if ( ctx == NULL || classNode == NULL || !xl6CgenClassHasValueDerive(classNode) ) {
        return;
    }
    xl6CgenClassPersistenceFromValueHelper(ctx, classNode);
    if ( xl6CgenClassHasFormatDerive(classNode, "json") ||
         xl6CgenClassHasFormatDerive(classNode, "xson") ) {
        xl6CgenClassPersistenceStringifyHelper(ctx, classNode, "json");
        xl6CgenClassPersistenceWriteHelper(ctx, classNode, "json");
    }
}

static int xl6CgenPersistenceGenericName(const char* name, const char** outFormat)
{
    if ( outFormat != NULL ) {
        *outFormat = NULL;
    }
    if ( xl6CTextEquals(name, "__xl_std_json_parse") || xl6CTextEquals(name, "__xl_std_json_read") ) {
        if ( outFormat != NULL ) {
            *outFormat = "json";
        }
        return 1;
    }
    if ( xl6CTextEquals(name, "__xl_std_xson_parse") || xl6CTextEquals(name, "__xl_std_xson_read") ) {
        if ( outFormat != NULL ) {
            *outFormat = "xson";
        }
        return 1;
    }
    return 0;
}

static int xl6CgenPersistenceStringifyName(const char* name, const char** outFormat)
{
    if ( outFormat != NULL ) {
        *outFormat = NULL;
    }
    if ( xl6CTextEquals(name, "__xl_std_json_stringify") ) {
        if ( outFormat != NULL ) {
            *outFormat = "json";
        }
        return 1;
    }
    if ( xl6CTextEquals(name, "__xl_std_xson_stringify") ) {
        if ( outFormat != NULL ) {
            *outFormat = "xson";
        }
        return 1;
    }
    return 0;
}

static int xl6CgenPersistenceWriteName(const char* name, const char** outFormat)
{
    if ( outFormat != NULL ) {
        *outFormat = NULL;
    }
    if ( xl6CTextEquals(name, "__xl_std_json_write") ) {
        if ( outFormat != NULL ) {
            *outFormat = "json";
        }
        return 1;
    }
    if ( xl6CTextEquals(name, "__xl_std_xson_write") ) {
        if ( outFormat != NULL ) {
            *outFormat = "xson";
        }
        return 1;
    }
    return 0;
}

static void xl6CgenPersistenceCallableSymbol(XL6_CgenContext* ctx, const XL6_AstNode* call, const char* name)
{
    XL6_AstNode* func = xl6CgenFindFunc(ctx, name);

    if ( func == NULL ) {
        xl6CgenError(ctx, call, "class persistence helper function is unavailable");
        xl6OutputPuts("0", ctx->out);
        return;
    }
    xl6CgenCallableTargetSymbol(ctx, func);
}

static const XL6_AstNode* xl6CgenPersistenceCallArg(const XL6_AstNode* node, const char* name, size_t index)
{
    const XL6_AstNode* args;
    size_t i;
    size_t positionalIndex;

    args = node != NULL && node->childCount > 1 ? node->children[1] : NULL;
    positionalIndex = 0;
    if ( args == NULL ) {
        return NULL;
    }
    for ( i = 0; i < args->childCount; ++i ) {
        const XL6_AstNode* arg = args->children[i];
        if ( arg == NULL || arg->childCount == 0 ) {
            continue;
        }
        if ( arg->kind == XL6_AST_NAMED_ARG ) {
            if ( xl6CTextEquals(arg->text, name) ) {
                return xl6CgenArgExpr(arg);
            }
            continue;
        }
        if ( positionalIndex == index ) {
            return xl6CgenArgExpr(arg);
        }
        positionalIndex += 1;
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenPersistenceGenericTypeArg(const XL6_AstNode* instant)
{
    const XL6_AstNode* typeArgs;

    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount < 2 ) {
        return NULL;
    }
    typeArgs = instant->children[1];
    if ( typeArgs == NULL || typeArgs->kind != XL6_AST_LIST || typeArgs->childCount != 1 ) {
        return NULL;
    }
    return typeArgs->children[0];
}

static int xl6CgenIsPersistenceGenericCall(const XL6_AstNode* node)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    instant = node->children[0];
    if ( instant == NULL || instant->kind != XL6_AST_GENERIC_INSTANT || instant->childCount < 2 ) {
        return 0;
    }
    nameNode = instant->children[0];
    return nameNode != NULL &&
           nameNode->kind == XL6_AST_NAME &&
           xl6CgenPersistenceGenericName(nameNode->text, NULL);
}

static XL6_AstNode* xl6CgenClonePersistenceGenericCallType(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* typeArg;

    (void)ctx;
    if ( !xl6CgenIsPersistenceGenericCall(node) ) {
        return NULL;
    }
    instant = node->children[0];
    typeArg = xl6CgenPersistenceGenericTypeArg(instant);
    return typeArg != NULL ? xl6CgenCloneTypeNode(typeArg) : NULL;
}

static const char* xl6CgenPersistenceGenericCallCType(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    XL6_AstNode* typeNode;
    const char* cType;

    typeNode = xl6CgenClonePersistenceGenericCallType(ctx, node);
    if ( typeNode == NULL ) {
        return NULL;
    }
    cType = xl6CgenTypeName(ctx, typeNode);
    xl6AstFree(typeNode);
    return cType;
}

static void xl6CgenPersistencePrettyExpr(XL6_CgenContext* ctx, const XL6_AstNode* node, size_t index)
{
    const XL6_AstNode* prettyExpr;

    prettyExpr = xl6CgenPersistenceCallArg(node, "pretty", index);
    if ( prettyExpr != NULL ) {
        xl6CgenExpr(ctx, prettyExpr);
    } else {
        xl6OutputPutc('0', ctx->out);
    }
}

static int xl6CgenPersistenceGenericCall(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* instant;
    const XL6_AstNode* nameNode;
    const XL6_AstNode* typeArg;
    const XL6_AstNode* textArg;
    XL6_AstNode* classNode;
    const char* format;

    if ( !xl6CgenIsPersistenceGenericCall(node) ) {
        return 0;
    }
    instant = node->children[0];
    nameNode = instant->children[0];
    format = NULL;
    xl6CgenPersistenceGenericName(nameNode != NULL ? nameNode->text : NULL, &format);
    typeArg = xl6CgenPersistenceGenericTypeArg(instant);
    classNode = xl6CgenPersistenceClassForType(ctx, typeArg);
    if ( classNode == NULL || !xl6CgenClassHasValueDerive(classNode) ) {
        xl6CgenError(ctx, node, "json/xson generic persistence target must derive value/json/xson");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }
    textArg = xl6CgenPersistenceCallArg(node,
        xl6CTextEquals(nameNode->text, "__xl_std_json_read") || xl6CTextEquals(nameNode->text, "__xl_std_xson_read") ? "path" : "text",
        0);
    if ( textArg == NULL ) {
        xl6CgenError(ctx, node, "json/xson generic persistence call requires one argument");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    xl6CgenClassPersistenceFromValueHelperSymbol(ctx->out, classNode);
    xl6OutputPutc('(', ctx->out);
    xl6CgenPersistenceCallableSymbol(ctx, node, nameNode->text);
    xl6OutputPutc('(', ctx->out);
    xl6CgenExpr(ctx, textArg);
    xl6OutputPuts("), ", ctx->out);
    xl6OutputPrintf(ctx->out, "%d)", xl6CgenPersistenceFormatKind(format));
    return 1;
}

static int xl6CgenPersistenceStdlibCall(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const char* funcName;
    const XL6_AstNode* valueExpr;
    const XL6_AstNode* pathExpr;
    XL6_AstNode* valueType;
    XL6_AstNode* classNode;
    const char* format;
    int isWrite;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ||
         node->children[0] == NULL ) {
        return 0;
    }
    callee = node->children[0];
    funcName = NULL;
    isWrite = 0;

    if ( callee->kind == XL6_AST_NAME ) {
        funcName = callee->text;
        if ( !xl6CgenPersistenceStringifyName(funcName, &format) ) {
            if ( !xl6CgenPersistenceWriteName(funcName, &format) ) {
                return 0;
            }
            isWrite = 1;
        }
    } else if ( callee->kind == XL6_AST_MEMBER && callee->childCount > 0 &&
                callee->children[0] != NULL && callee->children[0]->kind == XL6_AST_NAME ) {
        const char* moduleName = callee->children[0]->text;
        if ( xl6CTextEquals(moduleName, "json") && xl6CTextEquals(callee->text, "stringify") ) {
            format = "json";
            funcName = "__xl_std_json_stringify";
        } else if ( xl6CTextEquals(moduleName, "xson") && xl6CTextEquals(callee->text, "stringify") ) {
            format = "xson";
            funcName = "__xl_std_xson_stringify";
        } else if ( xl6CTextEquals(moduleName, "json") && xl6CTextEquals(callee->text, "write") ) {
            format = "json";
            funcName = "__xl_std_json_write";
            isWrite = 1;
        } else if ( xl6CTextEquals(moduleName, "xson") && xl6CTextEquals(callee->text, "write") ) {
            format = "xson";
            funcName = "__xl_std_xson_write";
            isWrite = 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }

    valueExpr = xl6CgenPersistenceCallArg(node, "value", isWrite ? 1 : 0);
    valueType = xl6CgenCloneExprTypeForInference(ctx, valueExpr);
    classNode = xl6CgenPersistenceClassForType(ctx, valueType);
    if ( classNode == NULL ) {
        xl6AstFree(valueType);
        return 0;
    }
    if ( !xl6CgenClassHasFormatDerive(classNode, format) ) {
        xl6CgenError(ctx, node, "json/xson class persistence call requires matching derive value/json/xson");
        xl6OutputPuts(isWrite ? "0" : "\"\"", ctx->out);
        xl6AstFree(valueType);
        return 1;
    }

    if ( isWrite ) {
        pathExpr = xl6CgenPersistenceCallArg(node, "path", 0);
        xl6CgenClassPersistenceFormatHelperSymbol(ctx->out, classNode, format, 1);
        xl6OutputPutc('(', ctx->out);
        xl6CgenPersistenceCallableSymbol(ctx, node, funcName);
        xl6OutputPuts(", ", ctx->out);
        if ( pathExpr != NULL ) {
            xl6CgenExpr(ctx, pathExpr);
        } else {
            xl6OutputPuts("\"\"", ctx->out);
        }
        xl6OutputPuts(", ", ctx->out);
        xl6CgenClassCopySourceExpr(ctx, valueExpr);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(xl6CgenExprIsClassBorrow(ctx, valueExpr, valueType) ? "1" : "0", ctx->out);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenPersistencePrettyExpr(ctx, node, 2);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPrintf(ctx->out, "%d", xl6CgenPersistenceFormatKind(format));
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenClassPersistenceFormatHelperSymbol(ctx->out, classNode, format, 0);
        xl6OutputPutc('(', ctx->out);
        xl6CgenPersistenceCallableSymbol(ctx, node, funcName);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenClassCopySourceExpr(ctx, valueExpr);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(xl6CgenExprIsClassBorrow(ctx, valueExpr, valueType) ? "1" : "0", ctx->out);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenPersistencePrettyExpr(ctx, node, 1);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPrintf(ctx->out, "%d", xl6CgenPersistenceFormatKind(format));
        xl6OutputPutc(')', ctx->out);
    }
    xl6AstFree(valueType);
    return 1;
}

