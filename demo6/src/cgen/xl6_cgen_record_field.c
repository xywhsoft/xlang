static void xl6CgenRecordFieldSelfBaseExpr(XL6_CgenContext* ctx, const char* basePath)
{
    const char* p;

    xl6OutputPuts("self", ctx->out);
    if ( basePath == NULL || basePath[0] == '\0' ) {
        return;
    }
    p = basePath;
    if ( *p == '.' ) {
        xl6OutputPuts("->", ctx->out);
        ++p;
    }
    while ( *p != '\0' ) {
        if ( *p == '.' ) {
            xl6OutputPutc('.', ctx->out);
        } else {
            xl6OutputPutc(*p, ctx->out);
        }
        ++p;
    }
}

static void xl6CgenRecordFieldSelfBaseAddressExpr(XL6_CgenContext* ctx, const char* basePath)
{
    if ( basePath == NULL || basePath[0] == '\0' ) {
        xl6OutputPuts("self", ctx->out);
        return;
    }
    xl6OutputPutc('&', ctx->out);
    xl6CgenRecordFieldSelfBaseExpr(ctx, basePath);
}

static int xl6CgenRecordFieldStaticGetCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* field)
{
    const XL6_AstNode* fieldType;
    char fieldPath[512];

    if ( ctx == NULL || classNode == NULL || field == NULL ||
         field->text == NULL || field->childCount == 0 ) {
        return 0;
    }

    fieldType = field->children[0];
    xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "(*self)", 0, field->text);

    xl6OutputPuts("        if ( strcmp(key, ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, field->text);
    xl6OutputPuts(") == 0 ) {\n", ctx->out);
    xl6OutputPuts("            result = ", ctx->out);
    xl6CgenClassValueExpr(ctx, fieldType, fieldPath);
    xl6OutputPuts(";\n", ctx->out);
    xl6OutputPuts("            goto done;\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
    return 1;
}

static int xl6CgenRecordFieldStaticSetCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* field)
{
    const XL6_AstNode* fieldType;
    char fieldPath[512];
    int prevIndent;

    if ( ctx == NULL || classNode == NULL || field == NULL ||
         field->text == NULL || field->childCount == 0 ) {
        return 0;
    }

    fieldType = field->children[0];
    xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "(*self)", 0, field->text);

    xl6OutputPuts("        if ( strcmp(key, ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, field->text);
    xl6OutputPuts(") == 0 ) {\n", ctx->out);
    xl6OutputPuts("            int ok = 1;\n", ctx->out);
    prevIndent = ctx->indent;
    ctx->indent = 3;
    xl6CgenClassAssignFromValue(ctx, fieldType, fieldPath, "value", "ok", NULL);
    ctx->indent = prevIndent;
    xl6OutputPuts("            return ok != 0;\n", ctx->out);
    xl6OutputPuts("        }\n", ctx->out);
    return 1;
}

static void xl6CgenRecordFieldProtocolGetCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode)
{
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    char basePath[256];

    ownerClass = NULL;
    method = xl6CgenFindClassProtocolFunc(ctx, classNode, "__field__", &ownerClass);
    if ( method == NULL || ownerClass == NULL ) {
        return;
    }
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        return;
    }

    xl6OutputPuts("        result = ", ctx->out);
    xl6CgenClassCallableSymbol(ctx, ownerClass, method);
    xl6OutputPuts("(", ctx->out);
    xl6CgenRecordFieldSelfBaseAddressExpr(ctx, basePath);
    xl6OutputPuts(", key);\n", ctx->out);
    xl6OutputPuts("        goto done;\n", ctx->out);
}

static void xl6CgenRecordFieldProtocolSetCase(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode)
{
    const XL6_AstNode* ownerClass;
    const XL6_AstNode* method;
    char basePath[256];

    ownerClass = NULL;
    method = xl6CgenFindClassProtocolFunc(ctx, classNode, "__setField__", &ownerClass);
    if ( method == NULL || ownerClass == NULL ) {
        return;
    }
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        return;
    }

    xl6OutputPuts("        ", ctx->out);
    xl6CgenClassCallableSymbol(ctx, ownerClass, method);
    xl6OutputPuts("(", ctx->out);
    xl6CgenRecordFieldSelfBaseAddressExpr(ctx, basePath);
    xl6OutputPuts(", key, value);\n", ctx->out);
    xl6OutputPuts("        return true;\n", ctx->out);
}

static void xl6CgenRecordFieldGetClassCase(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    char cTypeName[256];
    size_t i;

    if ( ctx == NULL || classNode == NULL || xl6CgenClassHasTypeParams(classNode) ||
         !xl6CgenClassWasEmitted(ctx, classNode->text) ) {
        return;
    }

    xl6CgenClassTypeTextName(cTypeName, sizeof(cTypeName), classNode->text);
    xl6OutputPuts("    if ( xrtTypeSame(desc, ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("()", ctx->out);
    xl6OutputPuts(") ) {\n", ctx->out);
    xl6OutputPuts("        ", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classNode), ctx->out);
    xl6OutputPuts("* self = (", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classNode), ctx->out);
    xl6OutputPuts("*)data;\n", ctx->out);

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        if ( xl6CgenIsInstanceField(body->children[i]) ) {
            xl6CgenRecordFieldStaticGetCase(ctx, classNode, body->children[i]);
        }
    }
    xl6CgenRecordFieldProtocolGetCase(ctx, classNode);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenRecordFieldSetClassCase(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    char cTypeName[256];
    size_t i;

    if ( ctx == NULL || classNode == NULL || xl6CgenClassHasTypeParams(classNode) ||
         !xl6CgenClassWasEmitted(ctx, classNode->text) ) {
        return;
    }

    xl6CgenClassTypeTextName(cTypeName, sizeof(cTypeName), classNode->text);
    xl6OutputPuts("    if ( xrtTypeSame(desc, ", ctx->out);
    xl6CgenTypeDescSymbol(ctx->out, cTypeName);
    xl6OutputPuts("()", ctx->out);
    xl6OutputPuts(") ) {\n", ctx->out);
    xl6OutputPuts("        ", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classNode), ctx->out);
    xl6OutputPuts("* self = (", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, classNode), ctx->out);
    xl6OutputPuts("*)data;\n", ctx->out);

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        if ( xl6CgenIsInstanceField(body->children[i]) ) {
            xl6CgenRecordFieldStaticSetCase(ctx, classNode, body->children[i]);
        }
    }
    xl6CgenRecordFieldProtocolSetCase(ctx, classNode);
    xl6OutputPuts("    }\n", ctx->out);
}

static void xl6CgenEmitRecordFieldHelpers(XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->out == NULL || ctx->root == NULL ) {
        return;
    }

    xl6OutputPuts("static xvalue xl6_record_field_get_owned(xvalue obj, const char* key, int release)\n", ctx->out);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPuts("    xvalue result = 0;\n", ctx->out);
    xl6OutputPuts("    const xrt_type_desc* desc = obj != 0 ? xvoTypeDesc(obj) : 0;\n", ctx->out);
    xl6OutputPuts("    void* data = obj != 0 ? xvoGetRecordData(obj) : 0;\n", ctx->out);
    xl6OutputPuts("    if ( desc != 0 && data != 0 && key != 0 ) {\n", ctx->out);
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        if ( node != NULL && node->kind == XL6_AST_CLASS ) {
            xl6CgenRecordFieldGetClassCase(ctx, node);
        }
    }
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    result = xl6_xrt_retain(xl6_xrt_index_safe_get_key(obj, key));\n", ctx->out);
    xl6OutputPuts("done:\n", ctx->out);
    xl6OutputPuts("    if ( release && obj != 0 ) { xvoUnref(obj); }\n", ctx->out);
    xl6OutputPuts("    return result != 0 ? result : xvoCreateNull();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);

    xl6OutputPuts("static bool xl6_record_field_set_borrow(xvalue obj, const char* key, xvalue value)\n", ctx->out);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPuts("    const xrt_type_desc* desc = obj != 0 ? xvoTypeDesc(obj) : 0;\n", ctx->out);
    xl6OutputPuts("    void* data = obj != 0 ? xvoGetRecordData(obj) : 0;\n", ctx->out);
    xl6OutputPuts("    if ( desc != 0 && data != 0 && key != 0 ) {\n", ctx->out);
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        if ( node != NULL && node->kind == XL6_AST_CLASS ) {
            xl6CgenRecordFieldSetClassCase(ctx, node);
        }
    }
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    return xvoTableSetValue(obj, key, 0, value, false);\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}
