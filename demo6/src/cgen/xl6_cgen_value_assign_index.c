static int xl6CgenIndexKeyIsXValue(XL6_CgenContext* ctx, const XL6_AstNode* index);
static int xl6CgenIndexKeyIsInteger(XL6_CgenContext* ctx, const XL6_AstNode* index);
static void xl6CgenXValueDictGetExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* receiverName, const XL6_AstNode* index);
static void xl6CgenXValueSafeIndexGetExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const char* receiverName, const XL6_AstNode* index);
static void xl6CgenTypedDictGetRawExpr(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index);
static void xl6CgenTypedDictSetDynamicKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName);
static void xl6CgenXValueDictSetDynamicKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName);
static void xl6CgenXValueIndexSetI64Stmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName);
static void xl6CgenXValueIndexSetDynamicKeyStmt(XL6_CgenContext* ctx, const XL6_AstNode* receiver, const XL6_AstNode* index, const char* tempName);

static void xl6CgenIndexAssignRaiseFailure(XL6_CgenContext* ctx, const XL6_AstNode* at, const char* message)
{
    xl6CgenIndent(ctx);
    if ( ctx != NULL && ctx->needsErrorRuntime ) {
        xl6OutputPrintf(ctx->out,
                "xl6_error_raise(\"RangeError\", \"%s\", \"\", %d, %d);\n",
                message != NULL ? message : "container index assignment failed",
                at != NULL ? at->line : 0,
                at != NULL ? at->column : 0);
    } else {
        xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
    }
}

static void xl6CgenIndexAssignTemp(XL6_CgenContext* ctx, const XL6_AstNode* left, const char* tempName)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* index;
    XL6_AstNode* receiverType;

    receiver = left != NULL && left->childCount > 0 ? left->children[0] : NULL;
    index = left != NULL && left->childCount > 1 ? left->children[1] : NULL;
    receiverType = xl6CgenCloneExprStorageType(ctx, receiver);

    if ( xl6CgenExprUsesTypedContainerStorage(ctx, receiver) && xl6CgenIsTypedContainerType(ctx, receiverType) ) {
        const XL6_AstNode* concreteType = xl6CgenTypedContainerConcreteType(ctx, receiverType);
        const char* typeName = concreteType != NULL ? concreteType->text : NULL;

        if ( xl6CTextEquals(typeName, "dict") && !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            if ( xl6CgenIndexKeyIsXValue(ctx, index) ) {
                xl6CgenTypedDictSetDynamicKeyStmt(ctx, receiver, index, tempName);
            } else {
                xl6CgenIndent(ctx);
                xl6CgenError(ctx, index, "dict index key must be string or var string");
                xl6OutputPuts("if (!false) {\n", ctx->out);
                ctx->indent += 1;
                xl6CgenIndexAssignRaiseFailure(ctx, index, "dict index assignment failed");
                ctx->indent -= 1;
                xl6CgenIndent(ctx);
                xl6OutputPuts("}\n", ctx->out);
            }
            xl6AstFree(receiverType);
            return;
        }

        xl6CgenIndent(ctx);
        xl6OutputPuts("if (!", ctx->out);
        if ( xl6CTextEquals(typeName, "array") ) {
            xl6OutputPuts("xl6_xrt_typed_array_set_at(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPrintf(ctx->out, "), %s)", tempName != NULL ? tempName : "0");
        } else if ( xl6CTextEquals(typeName, "list") ) {
            xl6OutputPuts("xrtTypedListSetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", (long long)(", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPrintf(ctx->out, "), %s)", tempName != NULL ? tempName : "0");
        } else if ( xl6CTextEquals(typeName, "dict") ) {
            xl6OutputPuts("xrtTypedDictSetValue(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPrintf(ctx->out, ", 0, %s)", tempName != NULL ? tempName : "0");
        } else {
            xl6CgenError(ctx, left, "typed container index assignment requires array, list or dict");
            xl6OutputPuts("false", ctx->out);
        }
        xl6OutputPuts(") {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndexAssignRaiseFailure(ctx, left, "container index assignment failed");
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        if ( tempName != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName);
        }
        xl6AstFree(receiverType);
        return;
    }

    if ( xl6CgenIsDictXValueType(receiverType) && !xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
        if ( xl6CgenIndexKeyIsXValue(ctx, index) ) {
            xl6CgenXValueDictSetDynamicKeyStmt(ctx, receiver, index, tempName);
        } else {
            xl6CgenIndent(ctx);
            xl6CgenError(ctx, index, "dict index key must be string or var string");
            xl6OutputPuts("if (false) {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
        xl6AstFree(receiverType);
        return;
    }

    if ( receiverType != NULL && xl6CTextEquals(receiverType->text, "var") ) {
        if ( xl6CgenExprStaticTypeIs(ctx, index, "string") ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts("if ( !xl6_record_field_set_borrow(", ctx->out);
            xl6CgenExpr(ctx, receiver);
            xl6OutputPuts(", ", ctx->out);
            xl6CgenExpr(ctx, index);
            xl6OutputPrintf(ctx->out, ", %s) ) {\n", tempName != NULL ? tempName : "0");
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if ( %s != 0 ) { xvoUnref(%s); }\n", tempName != NULL ? tempName : "0", tempName != NULL ? tempName : "0");
            xl6CgenIndexAssignRaiseFailure(ctx, left, "container index assignment failed");
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "else if ( %s != 0 ) { xvoUnref(%s); }\n", tempName != NULL ? tempName : "0", tempName != NULL ? tempName : "0");
        } else if ( xl6CgenIndexKeyIsXValue(ctx, index) ) {
            xl6CgenXValueIndexSetDynamicKeyStmt(ctx, receiver, index, tempName);
        } else if ( xl6CgenIndexKeyIsInteger(ctx, index) ) {
            xl6CgenXValueIndexSetI64Stmt(ctx, receiver, index, tempName);
        } else {
            xl6CgenIndent(ctx);
            xl6CgenError(ctx, index, "var container index key must be int, string or var");
            xl6OutputPuts("if (false) {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
        }
        xl6AstFree(receiverType);
        return;
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( !", ctx->out);
    if ( xl6CgenIsArrayXValueType(receiverType) ) {
        xl6OutputPuts("xvoIndexSetI64(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPrintf(ctx->out, "), %s, true)", tempName != NULL ? tempName : "0");
    } else if ( xl6CgenIsListXValueType(receiverType) ) {
        xl6OutputPuts("xvoListSetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", (long long)(", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPrintf(ctx->out, "), %s, true)", tempName != NULL ? tempName : "0");
    } else if ( xl6CgenIsDictXValueType(receiverType) ) {
        xl6OutputPuts("xvoTableSetValue(", ctx->out);
        xl6CgenExpr(ctx, receiver);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenExpr(ctx, index);
        xl6OutputPrintf(ctx->out, ", 0, %s, true)", tempName != NULL ? tempName : "0");
    } else {
        xl6CgenError(ctx, left, "dynamic index assignment receiver must be array, list or dict");
        xl6OutputPuts("false", ctx->out);
    }
    xl6OutputPrintf(ctx->out, " && %s != 0 ) {\n", tempName != NULL ? tempName : "0");
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvoUnref(%s);\n", tempName != NULL ? tempName : "0");
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    xl6AstFree(receiverType);
}
static void xl6CgenIndexAssignStmt(XL6_CgenContext* ctx, const XL6_AstNode* left, const XL6_AstNode* right)
{
    int id;
    char tempName[64];

    id = ctx->tempId++;
    snprintf(tempName, sizeof(tempName), "__xl6_index_assign_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = ", tempName);
    xl6CgenXValueFromExpr(ctx, right);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenIndexAssignTemp(ctx, left, tempName);
}

