static int xl6CgenClassNeedsAutoDeriveProtocol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* deriveName,
    const char* protocolName)
{
    if ( ctx == NULL || classNode == NULL || deriveName == NULL || protocolName == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasTypeParams(classNode) ) {
        return 0;
    }
    if ( !xl6CgenClassHasDerive(classNode, deriveName) ) {
        return 0;
    }
    return xl6CgenFindClassFunc(classNode, protocolName) == NULL;
}

static int xl6CgenFieldHasSkipAttr(const XL6_AstNode* field, const char* attrName)
{
    size_t i;

    if ( field == NULL || attrName == NULL ) {
        return 0;
    }
    for ( i = 1; i < field->childCount; ++i ) {
        const XL6_AstNode* attr = field->children[i];
        size_t j;

        if ( attr == NULL || attr->kind != XL6_AST_FIELD_ATTR || !xl6CTextEquals(attr->text, attrName) ) {
            continue;
        }
        for ( j = 0; j < attr->childCount; ++j ) {
            const XL6_AstNode* arg = attr->children[j];
            if ( arg != NULL && arg->kind == XL6_AST_NAME && xl6CTextEquals(arg->text, "skip") ) {
                return 1;
            }
        }
    }
    return 0;
}

static void xl6CgenClassAutoToStringProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6OutputPuts("const char* ", ctx->out);
    xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__toString__");
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts("* self)", ctx->out);
}

static void xl6CgenClassAutoHashProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6OutputPuts("long long ", ctx->out);
    xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__hash__");
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts("* self)", ctx->out);
}

static void xl6CgenClassAutoEqualsProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* protocolName)
{
    xl6OutputPuts("bool ", ctx->out);
    xl6CgenClassMemberSymbol(ctx->out, classNode->text, protocolName);
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts("* self, ", ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts(" other)", ctx->out);
}

static void xl6CgenClassAutoDeriveAppendLiteral(XL6_CgenContext* ctx, const char* resultName, const char* literal)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_next = (char*)xrtFormat(\"%%s%%s\", %s != 0 ? %s : \"\", ", resultName, resultName);
    xl6CgenCStringLiteral(ctx->out, literal != NULL ? literal : "");
    xl6OutputPuts(");\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtFree((void*)%s); }\n", resultName, resultName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = __xl6_next != 0 ? __xl6_next : xrtCopyStr(\"\", 0u);\n", resultName);
}

static void xl6CgenClassAutoDeriveAppendValue(XL6_CgenContext* ctx, const char* resultName)
{
    xl6CgenIndent(ctx);
    xl6OutputPuts("__xl6_text_size = 0u;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("__xl6_text = (char*)xvoToString(__xl6_value, &__xl6_text_size);\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_next = (char*)xrtFormat(\"%%s%%s\", %s != 0 ? %s : \"\", __xl6_text != 0 ? __xl6_text : \"\");\n", resultName, resultName);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if (__xl6_text != 0) { xrtFree(__xl6_text); __xl6_text = 0; }\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if (__xl6_value != 0) { xvoUnref(__xl6_value); __xl6_value = 0; }\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtFree((void*)%s); }\n", resultName, resultName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = __xl6_next != 0 ? __xl6_next : xrtCopyStr(\"\", 0u);\n", resultName);
}

static void xl6CgenClassAutoToStringProtocol(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;
    int wroteField;

    xl6CgenClassAutoToStringProto(ctx, classNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("const char* __xl6_result;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("char* __xl6_next;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("char* __xl6_text;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("unsigned int __xl6_text_size;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue __xl6_value;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( self == 0 ) { return xrtCopyStr(\"null\", 0u); }\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("__xl6_result = xrtCopyStr(", ctx->out);
    xl6CgenCStringLiteral(ctx->out, classNode != NULL ? classNode->text : "");
    xl6OutputPuts(", 0u);\n", ctx->out);
    xl6CgenClassAutoDeriveAppendLiteral(ctx, "__xl6_result", "{");

    body = xl6CgenClassBody(classNode);
    wroteField = 0;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        char fieldPath[512];

        if ( !xl6CgenIsInstanceField(field) || field->childCount == 0 || xl6CgenFieldHasSkipAttr(field, "string") ) {
            continue;
        }
        if ( wroteField ) {
            xl6CgenClassAutoDeriveAppendLiteral(ctx, "__xl6_result", ", ");
        }
        xl6CgenClassAutoDeriveAppendLiteral(ctx, "__xl6_result", field->text != NULL ? field->text : "");
        xl6CgenClassAutoDeriveAppendLiteral(ctx, "__xl6_result", "=");
        xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "self", 1, field->text);
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_value = ", ctx->out);
        xl6CgenClassValueExpr(ctx, field->children[0], fieldPath);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenClassAutoDeriveAppendValue(ctx, "__xl6_result");
        wroteField = 1;
    }

    xl6CgenClassAutoDeriveAppendLiteral(ctx, "__xl6_result", "}");
    xl6CgenIndent(ctx);
    xl6OutputPuts("return __xl6_result;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoHashProtocol(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    xl6CgenClassAutoHashProto(ctx, classNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("unsigned long long __xl6_hash;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("char* __xl6_text;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("unsigned int __xl6_text_size;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue __xl6_value;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( self == 0 ) { return 0; }\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("__xl6_hash = xrtHash64(", ctx->out);
    xl6CgenCStringLiteral(ctx->out, classNode != NULL ? classNode->text : "");
    xl6OutputPuts(", strlen(", ctx->out);
    xl6CgenCStringLiteral(ctx->out, classNode != NULL ? classNode->text : "");
    xl6OutputPuts("));\n", ctx->out);

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        char fieldPath[512];

        if ( !xl6CgenIsInstanceField(field) || field->childCount == 0 || xl6CgenFieldHasSkipAttr(field, "compare") ) {
            continue;
        }
        xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "self", 1, field->text);
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_value = ", ctx->out);
        xl6CgenClassValueExpr(ctx, field->children[0], fieldPath);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_text_size = 0u;\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_text = (char*)xvoToString(__xl6_value, &__xl6_text_size);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (__xl6_text != 0) { __xl6_hash ^= xrtHash64(__xl6_text, __xl6_text_size) + 0x9e3779b97f4a7c15ULL + (__xl6_hash << 6) + (__xl6_hash >> 2); xrtFree(__xl6_text); }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (__xl6_value != 0) { xvoUnref(__xl6_value); __xl6_value = 0; }\n", ctx->out);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("return (long long)__xl6_hash;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoEqualsBody(XL6_CgenContext* ctx, const XL6_AstNode* classNode, int negate)
{
    const XL6_AstNode* body;
    size_t i;

    xl6CgenClassAutoEqualsProto(ctx, classNode, negate ? "__ne__" : "__eq__");
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("int __xl6_equal = 1;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("char* __xl6_left_text;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("char* __xl6_right_text;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("unsigned int __xl6_left_size;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("unsigned int __xl6_right_size;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue __xl6_left;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue __xl6_right;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( self == 0 ) { return ", ctx->out);
    xl6OutputPuts(negate ? "true" : "false", ctx->out);
    xl6OutputPuts("; }\n", ctx->out);

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        char leftPath[512];
        char rightPath[512];

        if ( !xl6CgenIsInstanceField(field) || field->childCount == 0 || xl6CgenFieldHasSkipAttr(field, "compare") ) {
            continue;
        }
        xl6CgenClassFieldPath(leftPath, sizeof(leftPath), "self", 1, field->text);
        xl6CgenClassFieldPath(rightPath, sizeof(rightPath), "(&other)", 1, field->text);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (__xl6_equal) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_left = ", ctx->out);
        xl6CgenClassValueExpr(ctx, field->children[0], leftPath);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_right = ", ctx->out);
        xl6CgenClassValueExpr(ctx, field->children[0], rightPath);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_left_size = 0u; __xl6_right_size = 0u;\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_left_text = (char*)xvoToString(__xl6_left, &__xl6_left_size);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("__xl6_right_text = (char*)xvoToString(__xl6_right, &__xl6_right_size);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (__xl6_left_size != __xl6_right_size || (__xl6_left_size > 0u && memcmp(__xl6_left_text, __xl6_right_text, __xl6_left_size) != 0)) { __xl6_equal = 0; }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (__xl6_left_text != 0) { xrtFree(__xl6_left_text); }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (__xl6_right_text != 0) { xrtFree(__xl6_right_text); }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (__xl6_left != 0) { xvoUnref(__xl6_left); }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (__xl6_right != 0) { xvoUnref(__xl6_right); }\n", ctx->out);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("return ", ctx->out);
    xl6OutputPuts(negate ? "!__xl6_equal" : "__xl6_equal", ctx->out);
    xl6OutputPuts(";\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoEqualsProtocol(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6CgenClassAutoEqualsBody(ctx, classNode, 0);
    xl6CgenClassAutoEqualsBody(ctx, classNode, 1);
}

static int xl6CgenClassAutoEqualsBinaryExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiver,
    const XL6_AstNode* arg,
    const char* protocolName)
{
    XL6_AstNode* receiverType;
    XL6_AstNode* argType;
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;
    int result;

    if ( ctx == NULL || receiver == NULL || arg == NULL ||
         (!xl6CTextEquals(protocolName, "__eq__") && !xl6CTextEquals(protocolName, "__ne__")) ) {
        return 0;
    }
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    argType = xl6CgenCloneExprTypeForInference(ctx, arg);
    concreteType = xl6CgenResolveAliasType(ctx, receiverType);
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    classNode = concreteType != NULL && concreteType->text != NULL ? xl6CgenFindClass(ctx, concreteType->text) : NULL;
    result = classNode != NULL &&
             !xl6CgenClassHasTypeParams(classNode) &&
             xl6CgenClassNeedsAutoDeriveProtocol(ctx, classNode, "equals", protocolName) &&
             xl6CgenTypeNodeEqual(receiverType, argType);
    if ( result ) {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, protocolName);
        xl6OutputPutc('(', ctx->out);
        xl6CgenClassToStringSelfExpr(ctx, receiver, receiverType, concreteType);
        xl6OutputPuts(", ", ctx->out);
        if ( xl6CgenClassHandleTargetType(ctx, argType) != NULL ) {
            char cTypeName[512];

            xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), concreteType);
            xl6OutputPrintf(ctx->out, "(*((%s*)xl6_class_data(", cTypeName);
            xl6CgenExpr(ctx, arg);
            xl6OutputPrintf(ctx->out, ", \"\", %d, %d)))", arg->line, arg->column);
        } else {
            xl6CgenExpr(ctx, arg);
        }
        xl6OutputPutc(')', ctx->out);
    }
    xl6AstFree(receiverType);
    xl6AstFree(argType);
    return result;
}
