static int xl6CgenClassHasValueDerive(const XL6_AstNode* classNode)
{
    return xl6CgenClassHasDerive(classNode, "value") ||
           xl6CgenClassHasDerive(classNode, "json") ||
           xl6CgenClassHasDerive(classNode, "xson");
}

static int xl6CgenClassHasFormatDerive(const XL6_AstNode* classNode, const char* format)
{
    if ( !xl6CgenClassHasValueDerive(classNode) ) {
        return 0;
    }
    if ( xl6CTextEquals(format, "json") ) {
        return xl6CgenClassHasDerive(classNode, "value") ||
               xl6CgenClassHasDerive(classNode, "json");
    }
    if ( xl6CTextEquals(format, "xson") ) {
        return xl6CgenClassHasDerive(classNode, "value") ||
               xl6CgenClassHasDerive(classNode, "xson");
    }
    return 1;
}

static int xl6CgenClassNeedsAutoValueProtocol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* protocolName)
{
    if ( ctx == NULL || classNode == NULL || protocolName == NULL ) {
        return 0;
    }
    if ( xl6CgenClassHasTypeParams(classNode) || !xl6CgenClassHasValueDerive(classNode) ) {
        return 0;
    }
    return xl6CgenFindClassFunc(classNode, protocolName) == NULL;
}

static void xl6CgenClassAutoToValueProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6OutputPuts("xvalue ", ctx->out);
    xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__toValue__");
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts("* self)", ctx->out);
}

static void xl6CgenClassAutoFromValueProto(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6OutputPuts("int ", ctx->out);
    xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__fromValue__");
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts("* self, xvalue value)", ctx->out);
}

static void xl6CgenClassFieldPath(char* buffer, size_t capacity, const char* ownerPath, int pointer, const char* fieldName)
{
    char cField[256];

    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    xl6CgenAppendText(buffer, capacity, ownerPath != NULL ? ownerPath : "");
    xl6CgenAppendText(buffer, capacity, pointer ? "->" : ".");
    xl6CgenCIdentText(cField, sizeof(cField), fieldName);
    xl6CgenAppendText(buffer, capacity, cField);
}

static XL6_AstNode* xl6CgenPersistenceClassForType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    if ( typeNode != NULL && typeNode->kind == XL6_AST_CLASS ) {
        return (XL6_AstNode*)typeNode;
    }
    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return NULL;
    }
    if ( concreteType->kind == XL6_AST_TYPE && concreteType->childCount > 0 ) {
        return NULL;
    }
    return xl6CgenFindClass(ctx, concreteType->text);
}

static void xl6CgenEmitClassToValueSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* method;

    method = xl6CgenFindClassFunc(classNode, "__toValue__");
    if ( method != NULL ) {
        xl6CgenClassCallableSymbol(ctx, classNode, method);
        return;
    }
    if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__toValue__") ) {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__toValue__");
        return;
    }
    xl6CgenError(ctx, classNode, "class persistence requires __toValue__ or derive value/json/xson");
    xl6OutputPuts("xl6_missing_to_value", ctx->out);
}

static void xl6CgenEmitClassFromValueSymbol(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* method;

    method = xl6CgenFindClassFunc(classNode, "__fromValue__");
    if ( method != NULL ) {
        xl6CgenClassCallableSymbol(ctx, classNode, method);
        return;
    }
    if ( xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__fromValue__") ) {
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__fromValue__");
        return;
    }
    xl6CgenError(ctx, classNode, "class persistence requires __fromValue__ or derive value/json/xson");
    xl6OutputPuts("xl6_missing_from_value", ctx->out);
}

static void xl6CgenClassValueExpr(XL6_CgenContext* ctx, const XL6_AstNode* typeNode, const char* valuePath)
{
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL || concreteType->text == NULL ) {
        xl6OutputPuts("xvoCreateNull()", ctx->out);
        return;
    }
    if ( xl6CgenIsBoolTypeName(concreteType->text) ) {
        xl6OutputPuts("xvoCreateBool((bool)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CgenIsIntegerTypeName(concreteType->text) ) {
        xl6OutputPuts("xvoCreateInt((long long)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CgenIsFloatTypeName(concreteType->text) ) {
        xl6OutputPuts("xvoCreateFloat((double)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "string") ) {
        xl6OutputPuts("xvoCreateText((void*)((", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts(") != 0 ? (", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts(") : \"\"), 0, false)", ctx->out);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "time") ) {
        xl6OutputPuts("xvoCreateTime((long long)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "point") ) {
        xl6OutputPuts("xvoCreatePoint((void*)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPuts("xvoCopy(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    classNode = xl6CgenPersistenceClassForType(ctx, typeNode);
    if ( classNode != NULL && xl6CTextEquals(classNode->text, "object") ) {
        xl6OutputPuts("xvoCopy(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts(".__fields)", ctx->out);
        return;
    }
    if ( classNode != NULL ) {
        xl6CgenEmitClassToValueSymbol(ctx, classNode);
        xl6OutputPuts("(&", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    xl6OutputPuts("xvoCreateNull()", ctx->out);
}

static void xl6CgenClassAssignFromValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valuePath,
    const char* itemName,
    const char* okName)
{
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return;
    }
    if ( xl6CgenIsBoolTypeName(concreteType->text) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xvoGetBool(%s) ? 1 : 0;\n", valuePath, itemName);
        return;
    }
    if ( xl6CgenIsIntegerTypeName(concreteType->text) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = (%s)xvoGetInt(%s);\n", valuePath, xl6CgenTypeName(ctx, concreteType), itemName);
        return;
    }
    if ( xl6CgenIsFloatTypeName(concreteType->text) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = (%s)xvoGetFloat(%s);\n", valuePath, xl6CgenTypeName(ctx, concreteType), itemName);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "string") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xl6_xrt_text_temp_copy(xvoGetText(%s));\n", valuePath, itemName);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "time") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xvoGetTime(%s);\n", valuePath, itemName);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "point") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xvoGetPoint(%s);\n", valuePath, itemName);
        return;
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xvoUnref(%s); }\n", valuePath, valuePath);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xvoCopy(%s);\n", valuePath, itemName);
        return;
    }
    classNode = xl6CgenPersistenceClassForType(ctx, typeNode);
    if ( classNode != NULL && xl6CTextEquals(classNode->text, "object") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s.__fields != 0) { xvoUnref(%s.__fields); }\n", valuePath, valuePath);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s.__fields = xvoCopy(%s);\n", valuePath, itemName);
        return;
    }
    if ( classNode != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (!", ctx->out);
        xl6CgenEmitClassFromValueSymbol(ctx, classNode);
        xl6OutputPrintf(ctx->out, "(&%s, %s)) { %s = 0; }\n", valuePath, itemName, okName);
    }
}

static void xl6CgenClassAutoToValueProtocol(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    size_t i;

    xl6CgenClassAutoToValueProto(ctx, classNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue result = xvoCreateTable();\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue item = 0;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( self == 0 || result == 0 ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("return result;\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    base = classNode != NULL && classNode->childCount > 0 ? classNode->children[0] : NULL;
    baseClass = base != NULL && base->kind == XL6_AST_NAME ? xl6CgenFindClass(ctx, base->text) : NULL;
    if ( baseClass != NULL && xl6CTextEquals(baseClass->text, "object") ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( self->base.__fields != 0 ) { xvoTableMerge(result, self->base.__fields, false); }\n", ctx->out);
    } else if ( baseClass != NULL && xl6CgenClassHasValueDerive(baseClass) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("item = ", ctx->out);
        xl6CgenEmitClassToValueSymbol(ctx, baseClass);
        xl6OutputPuts("(&self->base);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( item != 0 && xvoType(item) == XL6_XVO_DT_TABLE ) { xvoTableMerge(result, item, false); }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( item != 0 ) { xvoUnref(item); item = 0; }\n", ctx->out);
    }

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* fieldType;
        char fieldPath[512];

        if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
            continue;
        }
        fieldType = member->children[0];
        xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "self", 1, member->text);
        xl6CgenIndent(ctx);
        xl6OutputPuts("item = ", ctx->out);
        xl6CgenClassValueExpr(ctx, fieldType, fieldPath);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( !xvoTableSetValue(result, ", ctx->out);
        xl6CgenCStringLiteral(ctx->out, member->text);
        xl6OutputPuts(", 0, item, true) && item != 0 ) { xvoUnref(item); }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("item = 0;\n", ctx->out);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("return result;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoFromValueProtocol(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    size_t i;

    xl6CgenClassAutoFromValueProto(ctx, classNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("int ok = 1;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue item = 0;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( self == 0 || value == 0 || xvoType(value) != XL6_XVO_DT_TABLE ) {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("return 0;\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    base = classNode != NULL && classNode->childCount > 0 ? classNode->children[0] : NULL;
    baseClass = base != NULL && base->kind == XL6_AST_NAME ? xl6CgenFindClass(ctx, base->text) : NULL;
    if ( baseClass != NULL && xl6CTextEquals(baseClass->text, "object") ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( self->base.__fields != 0 ) { xvoUnref(self->base.__fields); }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("self->base.__fields = xvoCopy(value);\n", ctx->out);
    } else if ( baseClass != NULL && xl6CgenClassHasValueDerive(baseClass) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (!", ctx->out);
        xl6CgenEmitClassFromValueSymbol(ctx, baseClass);
        xl6OutputPuts("(&self->base, value)) { ok = 0; }\n", ctx->out);
    }

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* fieldType;
        char fieldPath[512];

        if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
            continue;
        }
        fieldType = member->children[0];
        xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "self", 1, member->text);
        xl6CgenIndent(ctx);
        xl6OutputPuts("item = xvoTableGetValue(value, ", ctx->out);
        xl6CgenCStringLiteral(ctx->out, member->text);
        xl6OutputPuts(", 0);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( item != 0 && !xvoIsNull(item) ) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenClassAssignFromValue(ctx, fieldType, fieldPath, "item", "ok");
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("return ok;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

