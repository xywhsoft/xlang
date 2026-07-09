static void xl6CgenClassCopySourceExpr(XL6_CgenContext* ctx, const XL6_AstNode* expr)
{
    if ( expr != NULL &&
         expr->kind == XL6_AST_NAME &&
         xl6CTextEquals(expr->text, "self") &&
         ctx->currentClass != NULL ) {
        xl6OutputPuts("(*self)", ctx->out);
        return;
    }
    xl6CgenExpr(ctx, expr);
}

static int xl6CgenEmitClassCopyProtocolSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    int deep,
    const char** outProtocolName)
{
    const XL6_AstNode* method;

    method = NULL;
    if ( outProtocolName != NULL ) {
        *outProtocolName = NULL;
    }

    if ( deep ) {
        method = xl6CgenFindClassFunc(classNode, "__clone__");
        if ( method != NULL ) {
            xl6CgenClassCallableSymbol(ctx, classNode, method);
            if ( outProtocolName != NULL ) {
                *outProtocolName = "__clone__";
            }
            return 1;
        }
        if ( xl6CgenClassNeedsAutoCopyProtocol(ctx, classNode, "clone", "__clone__") ) {
            xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__clone__");
            if ( outProtocolName != NULL ) {
                *outProtocolName = "__clone__";
            }
            return 1;
        }
    }

    method = xl6CgenFindClassFunc(classNode, "__copy__");
    if ( method != NULL ) {
        xl6CgenClassCallableSymbol(ctx, classNode, method);
        if ( outProtocolName != NULL ) {
            *outProtocolName = "__copy__";
        }
        return 1;
    }
    if ( xl6CgenClassNeedsAutoCopyProtocol(ctx, classNode, "copy", "__copy__") ) {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__copy__");
        if ( outProtocolName != NULL ) {
            *outProtocolName = "__copy__";
        }
        return 1;
    }

    return 0;
}

static void xl6CgenClassCopyBuiltinHelperSymbol(XL6_Output* out, const XL6_AstNode* classNode, int deep)
{
    xl6CgenClassMemberSymbol(out, classNode != NULL ? classNode->text : "_", deep ? "__xl6_clone_value" : "__xl6_copy_value");
}

static void xl6CgenClassCopyBuiltinHelperProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, int deep)
{
    xl6OutputPuts("static ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenClassCopyBuiltinHelperSymbol(ctx->out, classNode, deep);
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(" source, int borrowed)", ctx->out);
}

static void xl6CgenClassCopyBuiltinHelper(XL6_CgenContext* ctx, const XL6_AstNode* classNode, int deep)
{
    const char* protocolName;
    int hasProtocol;

    xl6CgenClassCopyBuiltinHelperProto(ctx, classNode, deep);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;

    xl6CgenIndent(ctx);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(" result = {0};\n", ctx->out);

    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( borrowed ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenRetainClassFunctionFields(ctx, classNode, "source");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    xl6CgenIndent(ctx);
    xl6OutputPuts("result = ", ctx->out);
    protocolName = NULL;
    hasProtocol = xl6CgenEmitClassCopyProtocolSymbol(ctx, classNode, deep, &protocolName);
    if ( hasProtocol ) {
        xl6OutputPuts("(&source)", ctx->out);
    } else {
        xl6OutputPuts("source", ctx->out);
    }
    xl6OutputPuts(";\n", ctx->out);

    if ( !hasProtocol ) {
        if ( deep ) {
            xl6CgenDeepCopyClassFunctionFields(ctx, classNode, "result");
        } else {
            xl6CgenCopyClassFunctionFields(ctx, classNode, "result");
        }
    }
    if ( xl6CgenClassNeedsDestroy(ctx, classNode) ) {
        xl6CgenIndent(ctx);
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "destroy");
        xl6OutputPuts("(&source);\n", ctx->out);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("return result;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
    (void)protocolName;
}

static void xl6CgenClassCopyBuiltinHelperProtos(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    if ( ctx == NULL || classNode == NULL || xl6CgenClassHasTypeParams(classNode) ) {
        return;
    }
    xl6CgenClassCopyBuiltinHelperProto(ctx, classNode, 0);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenClassCopyBuiltinHelperProto(ctx, classNode, 1);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenClassCopyBuiltinHelpers(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    if ( ctx == NULL || classNode == NULL || xl6CgenClassHasTypeParams(classNode) ) {
        return;
    }
    xl6CgenClassCopyBuiltinHelper(ctx, classNode, 0);
    xl6CgenClassCopyBuiltinHelper(ctx, classNode, 1);
}

static void xl6CgenClassCopyBuiltinExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* arg,
    const XL6_AstNode* typeNode,
    const XL6_AstNode* classNode,
    int deep)
{
    (void)typeNode;
    xl6CgenClassCopyBuiltinHelperSymbol(ctx->out, classNode, deep);
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassCopySourceExpr(ctx, arg);
    xl6OutputPuts(", ", ctx->out);
    xl6OutputPuts(xl6CgenExprIsClassBorrow(ctx, arg, typeNode) ? "1" : "0", ctx->out);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenXValueCopyBuiltinExpr(XL6_CgenContext* ctx, const XL6_AstNode* arg, int deep)
{
    if ( xl6CgenExprIsXValueBorrow(ctx, arg) ) {
        xl6OutputPuts(deep ? "xvoDeepCopy(" : "xvoCopy(", ctx->out);
        xl6CgenExpr(ctx, arg);
        xl6OutputPutc(')', ctx->out);
        return;
    }

    xl6OutputPuts("xl6_xrt_value_copy_release(", ctx->out);
    xl6CgenExpr(ctx, arg);
    xl6OutputPuts(deep ? ", 1)" : ", 0)", ctx->out);
}

static void xl6CgenFunctionCopyBuiltinExpr(XL6_CgenContext* ctx, const XL6_AstNode* arg)
{
    xl6OutputPuts("xl6_function_copy_value(", ctx->out);
    xl6CgenExpr(ctx, arg);
    xl6OutputPutc(')', ctx->out);
}

static int xl6CgenCopyBuiltinIsShadowed(XL6_CgenContext* ctx, const char* name)
{
    if ( ctx == NULL || name == NULL ) {
        return 0;
    }
    if ( xl6CgenCopyBuiltinNameIsShadowed(ctx, name) ||
         xl6CgenFindExternFunc(ctx, name) != NULL ||
         xl6CgenFindExportFunc(ctx, name) != NULL ) {
        return 1;
    }
    return 0;
}

static int xl6CgenCopyBuiltinCall(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;
    const XL6_AstNode* argNode;
    const XL6_AstNode* arg;
    XL6_AstNode* typeNode;
    XL6_AstNode* classNode;
    int deep;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }
    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_NAME ||
         !xl6CgenIsCopyBuiltinName(callee->text) ) {
        return 0;
    }
    if ( xl6CgenCopyBuiltinIsShadowed(ctx, callee->text) ) {
        return 0;
    }

    deep = xl6CTextEquals(callee->text, "clone");
    args = node->childCount > 1 ? node->children[1] : NULL;
    argNode = args != NULL && args->childCount == 1 ? args->children[0] : NULL;
    arg = xl6CgenArgExpr(argNode);
    if ( arg == NULL || argNode == NULL || argNode->kind == XL6_AST_NAMED_ARG ) {
        xl6CgenError(ctx, node, "copy/clone requires one positional argument");
        xl6OutputPuts("0", ctx->out);
        return 1;
    }

    typeNode = xl6CgenCloneExprTypeForInference(ctx, arg);
    if ( typeNode == NULL ) {
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, arg);
        xl6OutputPutc(')', ctx->out);
        return 1;
    }

    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenXValueCopyBuiltinExpr(ctx, arg, deep);
        xl6AstFree(typeNode);
        return 1;
    }
    if ( xl6CgenIsTypedContainerType(ctx, typeNode) ) {
        xl6CgenTypedContainerCopyExpr(ctx, typeNode, arg);
        xl6AstFree(typeNode);
        return 1;
    }
    if ( xl6CgenIsFunctionType(typeNode) ) {
        xl6CgenFunctionCopyBuiltinExpr(ctx, arg);
        xl6AstFree(typeNode);
        return 1;
    }

    classNode = xl6CgenClassNodeForType(ctx, typeNode);
    if ( classNode != NULL ) {
        xl6CgenClassCopyBuiltinExpr(ctx, arg, typeNode, classNode, deep);
        xl6AstFree(typeNode);
        return 1;
    }

    xl6OutputPutc('(', ctx->out);
    xl6CgenExpr(ctx, arg);
    xl6OutputPutc(')', ctx->out);
    xl6AstFree(typeNode);
    return 1;
}

