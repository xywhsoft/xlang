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

/* 动态字段桥接保留 class 引用；JSON/XSON 转换只由显式持久化入口负责。 */
static void xl6CgenRecordFieldValueExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valuePath)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        xl6OutputPuts("xl6_xrt_retain(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6CgenClassValueExpr(ctx, typeNode, valuePath);
}

static void xl6CgenRecordFieldAssignFromValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* targetPath,
    const char* valuePath,
    const char* okName)
{
    xl6CgenClassAssignFromValue(ctx, typeNode, targetPath, valuePath, okName, NULL);
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
    xl6CgenRecordFieldValueExpr(ctx, fieldType, fieldPath);
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
    xl6CgenRecordFieldAssignFromValue(ctx, fieldType, fieldPath, "value", "ok");
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
    const XL6_AstNode* returns;
    const XL6_AstNode* returnType;
    char basePath[256];
    char valueName[64];
    int id;

    ownerClass = NULL;
    method = xl6CgenFindClassProtocolFunc(ctx, classNode, "__field__", &ownerClass);
    if ( method == NULL || ownerClass == NULL ) {
        return;
    }
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, classNode, ownerClass, basePath, sizeof(basePath)) ) {
        return;
    }

    returns = method->childCount > 2 ? method->children[2] : NULL;
    returnType = returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
    if ( returnType == NULL ) {
        return;
    }
    id = ctx->tempId++;
    snprintf(valueName, sizeof(valueName), "__xl6_record_field_result_%d", id);

    /* __field__ 可以返回任意可装箱类型，先保留真实 C 类型，再统一转换为 xvalue。 */
    xl6OutputPuts("        ", ctx->out);
    xl6OutputPuts(xl6CgenTypeName(ctx, returnType), ctx->out);
    xl6OutputPutc(' ', ctx->out);
    xl6OutputPuts(valueName, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    xl6CgenClassCallableSymbol(ctx, ownerClass, method);
    xl6OutputPuts("(", ctx->out);
    xl6CgenRecordFieldSelfBaseAddressExpr(ctx, basePath);
    xl6OutputPuts(", key);\n", ctx->out);
    xl6OutputPuts("        result = ", ctx->out);
    xl6CgenRecordFieldValueExpr(ctx, returnType, valueName);
    xl6OutputPuts(";\n", ctx->out);
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

    xl6OutputPuts("static xvalue xl6_record_field_get_mutable(xvalue obj, const char* key)\n", ctx->out);
    xl6OutputPuts("{\n", ctx->out);
    xl6OutputPuts("    return xvoIndexGetMutableKey(obj, (str)key, 0);\n", ctx->out);
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
