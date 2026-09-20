static int xl6CgenPersistenceFormatKind(const char* format);

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
    xl6OutputPuts("xvalue* ", ctx->out);
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
    xl6OutputPuts("* self, xvalue* value)", ctx->out);
}

static const char* xl6CgenClassValueFormatSuffix(const char* format)
{
    if ( xl6CTextEquals(format, "json") ) {
        return "__xl6_to_json_value";
    }
    if ( xl6CTextEquals(format, "xson") ) {
        return "__xl6_to_xson_value";
    }
    return "__xl6_to_value";
}

static const char* xl6CgenClassFromValueFormatSuffix(const char* format)
{
    if ( xl6CTextEquals(format, "json") ) {
        return "__xl6_from_json_value";
    }
    if ( xl6CTextEquals(format, "xson") ) {
        return "__xl6_from_xson_value";
    }
    return "__xl6_from_value";
}

static void xl6CgenClassAutoToValueFormatSymbol(
    XL6_Output* out,
    const XL6_AstNode* classNode,
    const char* format)
{
    xl6CgenClassMemberSymbol(out, classNode != NULL ? classNode->text : "_",
                             xl6CgenClassValueFormatSuffix(format));
}

static void xl6CgenClassAutoFromValueFormatSymbol(
    XL6_Output* out,
    const XL6_AstNode* classNode,
    const char* format)
{
    xl6CgenClassMemberSymbol(out, classNode != NULL ? classNode->text : "_",
                             xl6CgenClassFromValueFormatSuffix(format));
}

static void xl6CgenClassAutoToValueFormatProto(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* format)
{
    xl6OutputPuts("static xvalue* ", ctx->out);
    xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, format);
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts("* self)", ctx->out);
}

static void xl6CgenClassAutoFromValueFormatProto(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* format)
{
    xl6OutputPuts("static int ", ctx->out);
    xl6CgenClassAutoFromValueFormatSymbol(ctx->out, classNode, format);
    xl6OutputPutc('(', ctx->out);
    xl6CgenClassTypeName(ctx->out, classNode->text);
    xl6OutputPuts("* self, xvalue* value)", ctx->out);
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
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return NULL;
    }
    if ( concreteType->kind == XL6_AST_TYPE && concreteType->childCount > 0 ) {
        return NULL;
    }
    return xl6CgenFindClass(ctx, concreteType->text);
}

static const XL6_AstNode* xl6CgenPersistenceContainerItemType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* args;
    const char* name;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    name = concreteType != NULL ? concreteType->text : NULL;
    if ( !xl6CTextEquals(name, "array") &&
         !xl6CTextEquals(name, "list") &&
         !xl6CTextEquals(name, "set") &&
         !xl6CTextEquals(name, "dict") ) {
        return NULL;
    }
    args = concreteType->childCount > 0 ? concreteType->children[0] : NULL;
    return args != NULL && args->kind == XL6_AST_LIST && args->childCount == 1 ? args->children[0] : NULL;
}

static XL6_AstNode* xl6CgenPersistenceContainerItemClass(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* itemType;

    itemType = xl6CgenPersistenceContainerItemType(ctx, typeNode);
    return itemType != NULL ? xl6CgenPersistenceClassForType(ctx, itemType) : NULL;
}

static int xl6CgenPersistenceTypeIsTypedContainer(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6CgenPersistenceContainerItemType(ctx, typeNode) != NULL;
}

static int xl6CgenClassArrayAssignFromValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valuePath,
    const char* itemName,
    const char* okName,
    const char* format);

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
    const XL6_AstNode* recordType;
    XL6_AstNode* classNode;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL || concreteType->text == NULL ) {
        xl6OutputPuts("xrtValueNull()", ctx->out);
        return;
    }
    if ( xl6CgenIsNullableType(concreteType) ) {
        const XL6_AstNode* innerType = xl6CgenNullableInnerType(concreteType);
        char innerPath[1024];

        if ( xl6CgenNullableUsesTaggedValue(concreteType) ) {
            snprintf(innerPath, sizeof(innerPath), "(%s).value", valuePath);
            xl6OutputPrintf(ctx->out, "((%s).hasValue ? ", valuePath);
            xl6CgenClassValueExpr(ctx, innerType, innerPath);
            xl6OutputPuts(" : xrtValueNull())", ctx->out);
        } else if ( xl6CgenNullableUsesXValue(concreteType) ) {
            xl6OutputPrintf(
                ctx->out,
                "(((%s) != 0 && !xl6_xrt_value_is_null(%s)) ? ",
                valuePath,
                valuePath);
            xl6CgenClassValueExpr(ctx, innerType, valuePath);
            xl6OutputPuts(" : xrtValueNull())", ctx->out);
        } else {
            xl6OutputPrintf(ctx->out, "((%s) != 0 ? ", valuePath);
            xl6CgenClassValueExpr(ctx, innerType, valuePath);
            xl6OutputPuts(" : xrtValueNull())", ctx->out);
        }
        return;
    }
    if ( xl6CgenIsBoolTypeName(concreteType->text) ) {
        xl6OutputPuts("xrtValueBool((bool)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CgenIsIntegerTypeName(concreteType->text) ) {
        xl6OutputPuts("xrtValueInt((long long)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CgenIsFloatTypeName(concreteType->text) ) {
        xl6OutputPuts("xrtValueFloat((double)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "string") ) {
        xl6OutputPuts("xl6_xrt_value_text_create((void*)((", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts(") != 0 ? (", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts(") : \"\"), 0, false)", ctx->out);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "time") ) {
        xl6OutputPuts("xrtValueTime((long long)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "point") ) {
        xl6OutputPuts("xrtValuePointer((void*)(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPuts("))", ctx->out);
        return;
    }
    if ( xl6CgenIsTypedContainerType(ctx, concreteType) ) {
        const XL6_AstNode* itemType = xl6CgenPersistenceContainerItemType(ctx, concreteType);
        if ( xl6CTextEquals(concreteType->text, "array") ) {
            xl6OutputPuts("xl6_xrt_typed_array_to_xvalue(", ctx->out);
        } else if ( xl6CTextEquals(concreteType->text, "list") ) {
            xl6OutputPuts("xl6_xrt_typed_list_to_xvalue(", ctx->out);
        } else if ( xl6CTextEquals(concreteType->text, "set") ) {
            xl6OutputPuts("xl6_xrt_typed_set_to_xvalue(", ctx->out);
        } else {
            xl6OutputPuts("xl6_xrt_typed_dict_to_xvalue(", ctx->out);
        }
        xl6CgenEmitXrtTypeDescRef(ctx, itemType);
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    recordType = xl6CgenClassHandleTargetType(ctx, concreteType);
    if ( recordType != NULL ) {
        char cTypeName[512];

        classNode = xl6CgenPersistenceClassForType(ctx, recordType);
        xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), recordType);
        if ( classNode != NULL && xl6CTextEquals(classNode->text, "object") ) {
            xl6OutputPuts("xl6_xrt_dynamic_fields_to_value_handle(((", ctx->out);
            xl6OutputPuts(cTypeName, ctx->out);
            xl6OutputPuts("*)xl6ValueRecordData(", ctx->out);
            xl6OutputPuts(valuePath, ctx->out);
            xl6OutputPuts("))->__fields)", ctx->out);
            return;
        }
        if ( classNode != NULL ) {
            xl6CgenEmitClassToValueSymbol(ctx, classNode);
            xl6OutputPuts("((", ctx->out);
            xl6OutputPuts(cTypeName, ctx->out);
            xl6OutputPuts("*)xl6ValueRecordData(", ctx->out);
            xl6OutputPuts(valuePath, ctx->out);
            xl6OutputPuts("))", ctx->out);
            return;
        }
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6OutputPuts("xrtValueClone(", ctx->out);
        xl6OutputPuts(valuePath, ctx->out);
        xl6OutputPutc(')', ctx->out);
        return;
    }
    classNode = xl6CgenPersistenceClassForType(ctx, typeNode);
    if ( classNode != NULL && xl6CTextEquals(classNode->text, "object") ) {
        xl6OutputPuts("xl6_xrt_dynamic_fields_to_value_handle(", ctx->out);
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
    xl6OutputPuts("xrtValueNull()", ctx->out);
}

static void xl6CgenClassClearNullableValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* nullableType,
    const char* valuePath)
{
    const XL6_AstNode* innerType = xl6CgenNullableInnerType(nullableType);

    if ( innerType == NULL ) {
        return;
    }
    if ( xl6CgenNullableUsesTaggedValue(nullableType) ) {
        xl6CgenIndent(ctx);
        if ( xl6CgenNullableHasManagedInner(ctx, nullableType) ) {
            char dropName[64];
            xl6CgenNullableDropHelperName(dropName, sizeof(dropName), nullableType);
            xl6OutputPrintf(ctx->out, "%s(&%s);\n", dropName, valuePath);
        } else {
            xl6OutputPrintf(ctx->out, "memset(&%s, 0, sizeof(%s));\n", valuePath, valuePath);
        }
        return;
    }
    if ( xl6CgenNullableUsesXValue(nullableType) || xl6CgenIsXValueType(innerType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "if (%s != 0) { xrtValueRelease(%s); } %s = xrtValueNull();\n",
            valuePath,
            valuePath,
            valuePath);
        return;
    }
    if ( xl6CgenIsTypedContainerType(ctx, innerType) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) {\n", valuePath);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6CgenTypedContainerDestroyValue(ctx, innerType, valuePath);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", valuePath);
        return;
    }
    if ( xl6CTextEquals(innerType->text, "string") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "if (%s != 0) { xrtFree((void*)%s); } %s = 0;\n",
            valuePath,
            valuePath,
            valuePath);
        return;
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = 0;\n", valuePath);
}

static void xl6CgenClassAssignFromValue(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valuePath,
    const char* itemName,
    const char* okName,
    const char* format)
{
    const XL6_AstNode* concreteType;
    XL6_AstNode* classNode;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return;
    }
    if ( xl6CgenIsNullableType(concreteType) ) {
        const XL6_AstNode* innerType = xl6CgenNullableInnerType(concreteType);

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s == 0 || xl6_xrt_value_is_null(%s)) {\n", itemName, itemName);
        ctx->indent += 1;
        xl6CgenClassClearNullableValue(ctx, concreteType, valuePath);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        if ( xl6CgenNullableUsesTaggedValue(concreteType) ) {
            char innerPath[1024];
            snprintf(innerPath, sizeof(innerPath), "(%s).value", valuePath);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "(%s).hasValue = true;\n", valuePath);
            xl6CgenClassAssignFromValue(
                ctx,
                innerType,
                innerPath,
                itemName,
                okName,
                format);
        } else {
            xl6CgenClassAssignFromValue(
                ctx,
                innerType,
                valuePath,
                itemName,
                okName,
                format);
        }
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( xl6CgenIsBoolTypeName(concreteType->text) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xl6_xrt_value_bool(%s) ? 1 : 0;\n", valuePath, itemName);
        return;
    }
    if ( xl6CgenIsIntegerTypeName(concreteType->text) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = (%s)xl6_xrt_value_int(%s);\n", valuePath, xl6CgenTypeName(ctx, concreteType), itemName);
        return;
    }
    if ( xl6CgenIsFloatTypeName(concreteType->text) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = (%s)xl6_xrt_value_float(%s);\n", valuePath, xl6CgenTypeName(ctx, concreteType), itemName);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "string") ) {
        int id = ctx->tempId++;

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "char* __xl6_string_from_value_%d = xl6_xrt_string_dup(xl6_xrt_value_text(%s), 0u);\n", id, itemName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_string_from_value_%d == 0) { %s = 0; } else {\n", id, okName);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xrtFree((void*)%s);\n", valuePath);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = __xl6_string_from_value_%d;\n", valuePath, id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "time") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xl6_xrt_value_time(%s);\n", valuePath, itemName);
        return;
    }
    if ( xl6CTextEquals(concreteType->text, "point") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xl6_xrt_value_pointer(%s);\n", valuePath, itemName);
        return;
    }
    if ( xl6CgenClassArrayAssignFromValueStmt(ctx, concreteType, valuePath, itemName, okName, format) ) {
        return;
    }
    if ( xl6CgenClassHandleTargetType(ctx, typeNode) != NULL ) {
        const XL6_AstNode* recordType = xl6CgenClassHandleTargetType(ctx, typeNode);
        char cTypeName[512];
        int id = ctx->tempId++;

        classNode = xl6CgenPersistenceClassForType(ctx, typeNode);
        if ( classNode == NULL || !xl6CgenClassHasValueDerive(classNode) ) {
            xl6CgenClassTypeRefName(cTypeName, sizeof(cTypeName), recordType);
            xl6CgenIndent(ctx);
            xl6OutputPuts("if (", ctx->out);
            xl6OutputPuts(itemName, ctx->out);
            xl6OutputPuts(" == 0 || !xl6TypeIsA(xl6ValueType(", ctx->out);
            xl6OutputPuts(itemName, ctx->out);
            xl6OutputPuts("), ", ctx->out);
            xl6CgenTypeDescSymbol(ctx->out, cTypeName);
            xl6OutputPrintf(ctx->out, "())) { %s = 0; } else {\n", okName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRelease(%s); }\n", valuePath, valuePath);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = xl6_xrt_retain(%s);\n", valuePath, itemName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            return;
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue* __xl6_class_from_value_%d = ", id);
        xl6CgenClassMemberSymbol(ctx->out, classNode->text, "__xl6_from_value_release");
        /* 反序列化只读取子值；保留同一引用，避免浅拷贝容器 backing。 */
        xl6OutputPrintf(ctx->out, "(xl6_xrt_retain(%s), %d);\n", itemName, xl6CgenPersistenceFormatKind(format));
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_class_from_value_%d == 0 || xl6_xrt_value_is_null(__xl6_class_from_value_%d)) {\n", id, id);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_class_from_value_%d != 0) { xrtValueRelease(__xl6_class_from_value_%d); }\n", id, id);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", okName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRelease(%s); }\n", valuePath, valuePath);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = __xl6_class_from_value_%d;\n", valuePath, id);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        return;
    }
    if ( xl6CgenIsXValueType(typeNode) ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRelease(%s); }\n", valuePath, valuePath);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = xrtValueClone(%s);\n", valuePath, itemName);
        return;
    }
    classNode = xl6CgenPersistenceClassForType(ctx, typeNode);
    if ( classNode != NULL && xl6CTextEquals(classNode->text, "object") ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
            ctx->out,
            "if (!xl6_xrt_dynamic_fields_assign_value_handle(%s.__fields, %s)) { %s = 0; }\n",
            valuePath,
            itemName,
            okName);
        return;
    }
    if ( classNode != NULL ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("if (!", ctx->out);
        xl6CgenEmitClassFromValueSymbol(ctx, classNode);
        xl6OutputPrintf(ctx->out, "(&%s, %s)) { %s = 0; }\n", valuePath, itemName, okName);
    }
}

typedef struct XL6_CgenFieldPersistConfig {
    int skip;
    int include;
    int required;
    int omitEmpty;
    int omitNull;
    const XL6_AstNode* nameNode;
} XL6_CgenFieldPersistConfig;

static void xl6CgenApplyFieldPersistAttr(XL6_CgenFieldPersistConfig* cfg, const XL6_AstNode* attr)
{
    size_t i;

    if ( cfg == NULL || attr == NULL ) {
        return;
    }
    for ( i = 0; i < attr->childCount; ++i ) {
        const XL6_AstNode* arg = attr->children[i];
        const XL6_AstNode* key;
        const XL6_AstNode* value;

        if ( arg == NULL ) {
            continue;
        }
        if ( arg->kind == XL6_AST_STRING ) {
            cfg->nameNode = arg;
            continue;
        }
        if ( arg->kind == XL6_AST_NAME ) {
            if ( xl6CTextEquals(arg->text, "skip") ) {
                cfg->skip = 1;
            } else if ( xl6CTextEquals(arg->text, "include") ) {
                cfg->include = 1;
            } else if ( xl6CTextEquals(arg->text, "required") ) {
                cfg->required = 1;
            }
            continue;
        }
        if ( arg->kind != XL6_AST_KEY_VALUE || arg->childCount < 2 ) {
            continue;
        }
        key = arg->children[0];
        value = arg->children[1];
        if ( key == NULL || value == NULL ) {
            continue;
        }
        if ( xl6CTextEquals(key->text, "name") && value->kind == XL6_AST_STRING ) {
            cfg->nameNode = value;
        } else if ( xl6CTextEquals(key->text, "omit") && value->kind == XL6_AST_NAME ) {
            if ( xl6CTextEquals(value->text, "empty") ) {
                cfg->omitEmpty = 1;
            } else if ( xl6CTextEquals(value->text, "null") ) {
                cfg->omitNull = 1;
            }
        }
    }
}

static void xl6CgenCollectFieldPersistConfig(
    const XL6_AstNode* field,
    const char* format,
    XL6_CgenFieldPersistConfig* cfg)
{
    size_t i;

    memset(cfg, 0, sizeof(*cfg));
    for ( i = 1; field != NULL && i < field->childCount; ++i ) {
        const XL6_AstNode* attr = field->children[i];
        if ( attr != NULL && attr->kind == XL6_AST_FIELD_ATTR && xl6CTextEquals(attr->text, "persist") ) {
            xl6CgenApplyFieldPersistAttr(cfg, attr);
        }
    }
    for ( i = 1; field != NULL && i < field->childCount; ++i ) {
        const XL6_AstNode* attr = field->children[i];
        if ( attr != NULL && attr->kind == XL6_AST_FIELD_ATTR &&
             format != NULL && xl6CTextEquals(attr->text, format) ) {
            xl6CgenApplyFieldPersistAttr(cfg, attr);
        }
    }
}

static int xl6CgenFieldPersistIncluded(const XL6_AstNode* field, const XL6_CgenFieldPersistConfig* cfg)
{
    if ( field == NULL || cfg == NULL || cfg->skip ) {
        return 0;
    }
    if ( (field->flags & XL6_AST_FLAG_PRIVATE) && !cfg->include ) {
        return 0;
    }
    return 1;
}

static void xl6CgenFieldPersistKeyLiteral(
    XL6_CgenContext* ctx,
    const XL6_AstNode* field,
    const XL6_CgenFieldPersistConfig* cfg)
{
    if ( cfg != NULL && cfg->nameNode != NULL ) {
        xl6CgenStringLiteralExpr(ctx, cfg->nameNode);
        return;
    }
    xl6CgenCStringLiteral(ctx->out, field != NULL ? field->text : "");
}

static void xl6CgenClassPersistSetField(
    XL6_CgenContext* ctx,
    const XL6_AstNode* field,
    const XL6_CgenFieldPersistConfig* cfg)
{
    xl6CgenIndent(ctx);
    if ( cfg != NULL && (cfg->omitEmpty || cfg->omitNull) ) {
        if ( cfg->omitEmpty ) {
            xl6OutputPuts("if ( item != 0 && xl6_xrt_value_truthy(item) ) {\n", ctx->out);
        } else {
            xl6OutputPuts("if ( item != 0 && !xl6_xrt_value_is_null(item) ) {\n", ctx->out);
        }
        ctx->indent += 1;
        xl6CgenIndent(ctx);
    }
    xl6OutputPuts("if ( !xl6_xrt_value_object_set(result, ", ctx->out);
    xl6CgenFieldPersistKeyLiteral(ctx, field, cfg);
    xl6OutputPuts(", 0, item, true) && item != 0 ) { xrtValueRelease(item); }\n", ctx->out);
    if ( cfg != NULL && (cfg->omitEmpty || cfg->omitNull) ) {
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else if ( item != 0 ) { xrtValueRelease(item); }\n", ctx->out);
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts("item = 0;\n", ctx->out);
}

static void xl6CgenEmitClassToValueForFormat(
    XL6_CgenContext* ctx,
    XL6_AstNode* classNode,
    const char* format)
{
    if ( classNode != NULL && xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__toValue__") ) {
        if ( xl6CTextEquals(format, "json") ) {
            xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, "json");
        } else if ( xl6CTextEquals(format, "xson") ) {
            xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, "xson");
        } else {
            xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, NULL);
        }
        return;
    }
    xl6CgenEmitClassToValueSymbol(ctx, classNode);
}

static int xl6CgenClassArrayValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valuePath,
    const char* itemName,
    const char* format)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const XL6_AstNode* itemRecordType;
    XL6_AstNode* itemClass;
    char itemCType[512];
    int id;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    if ( concreteType == NULL ||
         !xl6CgenIsTypedContainerType(ctx, concreteType) ||
         !xl6CTextEquals(concreteType->text, "array") ) {
        return 0;
    }
    itemType = xl6CgenPersistenceContainerItemType(ctx, concreteType);
    itemClass = xl6CgenPersistenceContainerItemClass(ctx, concreteType);
    if ( itemType == NULL || itemClass == NULL ) {
        return 0;
    }
    itemRecordType = xl6CgenClassHandleTargetType(ctx, itemType);
    if ( itemRecordType == NULL ) {
        return 0;
    }
    xl6CgenClassTypeRefName(itemCType, sizeof(itemCType), itemRecordType);

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* __xl6_array_value_%d = xrtValueArray();\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "unsigned int __xl6_array_count_%d = xl6TypedArrayCount(%s);\n", id, valuePath);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "unsigned int __xl6_array_i_%d;\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "for (__xl6_array_i_%d = 0; __xl6_array_i_%d < __xl6_array_count_%d; ++__xl6_array_i_%d) {\n",
                    id, id, id, id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue*", ctx->out);
    xl6OutputPrintf(ctx->out, "* __xl6_array_item_%d = (", id);
    xl6OutputPuts("xvalue*", ctx->out);
    xl6OutputPrintf(ctx->out, "*)xl6_xrt_typed_array_get_at(%s, __xl6_array_i_%d);\n", valuePath, id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* __xl6_array_item_value_%d = (__xl6_array_item_%d != 0 && *__xl6_array_item_%d != 0) ? ", id, id, id);
    xl6CgenEmitClassToValueForFormat(ctx, itemClass, format);
    xl6OutputPrintf(ctx->out, "((%s*)xl6_class_data(*__xl6_array_item_%d, \"\", 0, 0)) : xrtValueNull();\n", itemCType, id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
                    "if ( !xl6_xrt_value_array_append(__xl6_array_value_%d, __xl6_array_item_value_%d, true) && __xl6_array_item_value_%d != 0 ) { xrtValueRelease(__xl6_array_item_value_%d); }\n",
                    id, id, id, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = __xl6_array_value_%d != 0 ? __xl6_array_value_%d : xrtValueNull();\n", itemName, id, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static void xl6CgenEmitClassFromValueForFormat(
    XL6_CgenContext* ctx,
    XL6_AstNode* classNode,
    const char* format)
{
    if ( classNode != NULL && xl6CgenClassNeedsAutoValueProtocol(ctx, classNode, "__fromValue__") ) {
        if ( xl6CTextEquals(format, "json") ) {
            xl6CgenClassAutoFromValueFormatSymbol(ctx->out, classNode, "json");
        } else if ( xl6CTextEquals(format, "xson") ) {
            xl6CgenClassAutoFromValueFormatSymbol(ctx->out, classNode, "xson");
        } else {
            xl6CgenClassAutoFromValueFormatSymbol(ctx->out, classNode, NULL);
        }
        return;
    }
    xl6CgenEmitClassFromValueSymbol(ctx, classNode);
}

static int xl6CgenClassArrayAssignFromValueStmt(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valuePath,
    const char* itemName,
    const char* okName,
    const char* format)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* itemType;
    const XL6_AstNode* itemRecordType;
    XL6_AstNode* itemClass;
    char itemCType[512];
    int itemHasFromValue;
    int id;

    concreteType = xl6CgenResolveAliasType(ctx, xl6CgenSubstituteGenericType(ctx, typeNode));
    /* array<var> 由 xvalue* 保存，不能套用 xtarray 的销毁与重建规则。 */
    if ( concreteType == NULL ||
         !xl6CgenIsTypedContainerType(ctx, concreteType) ||
         !xl6CTextEquals(concreteType->text, "array") ) {
        return 0;
    }
    itemType = xl6CgenPersistenceContainerItemType(ctx, concreteType);
    if ( itemType == NULL ) {
        return 0;
    }
    itemClass = xl6CgenPersistenceContainerItemClass(ctx, concreteType);
    itemRecordType = itemClass != NULL ? xl6CgenClassHandleTargetType(ctx, itemType) : NULL;
    if ( itemClass != NULL && itemRecordType == NULL ) {
        return 0;
    }
    itemCType[0] = '\0';
    itemHasFromValue = 0;
    if ( itemRecordType != NULL ) {
        xl6CgenClassTypeRefName(itemCType, sizeof(itemCType), itemRecordType);
        itemHasFromValue = xl6CgenFindClassFunc(itemClass, "__fromValue__") != NULL ||
                           xl6CgenClassHasValueDerive(itemClass);
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s != 0) { xl6TypedArrayRelease(%s); }\n", valuePath, valuePath);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = xl6TypedArrayCreate(", valuePath);
    xl6CgenEmitXrtTypeDescRef(ctx, itemType);
    xl6OutputPuts(");\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0 || %s == 0 || xl6_xrt_value_kind(%s) != XL6_XVO_DT_ARRAY) {\n", valuePath, itemName, itemName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = 0;\n", okName);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("} else {\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
	xl6OutputPrintf(ctx->out, "size_t __xl6_array_count_%d = xrtValueCount(%s);\n", id, itemName);
	xl6CgenIndent(ctx);
	xl6OutputPrintf(ctx->out, "size_t __xl6_array_i_%d;\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "for (__xl6_array_i_%d = 0; __xl6_array_i_%d < __xl6_array_count_%d; ++__xl6_array_i_%d) {\n",
                    id, id, id, id);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue* __xl6_array_item_value_%d = xrtValueArrayGet(%s, __xl6_array_i_%d);\n", id, itemName, id);
    if ( itemClass != NULL ) {
        char elemName[64];
        char boxedName[64];
        char targetExpr[96];
        snprintf(elemName, sizeof(elemName), "__xl6_array_item_%d", id);
        snprintf(boxedName, sizeof(boxedName), "__xl6_array_item_boxed_%d", id);
        snprintf(targetExpr, sizeof(targetExpr), "(*%s)", elemName);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_array_item_value_%d == 0 || xl6_xrt_value_is_null(__xl6_array_item_value_%d) || xl6TypeIsA(xl6ValueType(__xl6_array_item_value_%d), ", id, id, id);
        xl6CgenTypeDescSymbol(ctx->out, itemCType);
        xl6OutputPuts("())) {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (!xl6TypedArrayAppendValue(%s, __xl6_array_item_value_%d)) { %s = 0; }\n",
                        valuePath, id, okName);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("} else {\n", ctx->out);
        ctx->indent += 1;
        if ( itemHasFromValue ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "xvalue* %s = xl6ValueObject(", boxedName);
            xl6CgenTypeDescSymbol(ctx->out, itemCType);
            xl6OutputPuts("());\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPuts(itemCType, ctx->out);
            xl6OutputPrintf(ctx->out,
                            "* %s = %s != 0 ? (%s*)xl6ValueRecordData(%s) : 0;\n",
                            elemName,
                            boxedName,
                            itemCType,
                            boxedName);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s == 0) {\n", elemName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = 0;\n", okName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("} else {\n", ctx->out);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = (%s){0};\n", targetExpr, itemCType);
            xl6CgenClassDefaultInitFields(ctx, itemClass, targetExpr);
            xl6CgenIndent(ctx);
            xl6OutputPuts("if (!", ctx->out);
            xl6CgenEmitClassFromValueForFormat(ctx, itemClass, format);
            xl6OutputPrintf(ctx->out, "(%s, __xl6_array_item_value_%d)) { %s = 0; } else {\n", elemName, id, okName);
            ctx->indent += 1;
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (!xl6TypedArrayAppendValue(%s, %s)) { %s = 0; }\n", valuePath, boxedName, okName);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            ctx->indent -= 1;
            xl6CgenIndent(ctx);
            xl6OutputPuts("}\n", ctx->out);
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s != 0) { xrtValueRelease(%s); }\n", boxedName, boxedName);
        } else {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "%s = 0;\n", okName);
        }
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    } else {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (!xl6TypedArrayAppendValue(%s, __xl6_array_item_value_%d)) { %s = 0; }\n",
                        valuePath, id, okName);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    return 1;
}

static void xl6CgenClassAutoToValueFormatProtocol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* format)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    size_t i;

    xl6CgenClassAutoToValueFormatProto(ctx, classNode, format);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue* result = xrtValueObject();\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue* item = 0;\n", ctx->out);
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
        xl6OutputPuts("item = xl6_xrt_dynamic_fields_to_value_handle(self->base.__fields);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( item != 0 && xl6_xrt_value_kind(item) == XL6_XVO_DT_TABLE ) { xrtValueObjectMerge(result, item, 0); }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( item != 0 ) { xrtValueRelease(item); item = 0; }\n", ctx->out);
    } else if ( baseClass != NULL && xl6CgenClassHasValueDerive(baseClass) ) {
        xl6CgenIndent(ctx);
        xl6OutputPuts("item = ", ctx->out);
        xl6CgenEmitClassToValueSymbol(ctx, baseClass);
        xl6OutputPuts("(&self->base);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( item != 0 && xl6_xrt_value_kind(item) == XL6_XVO_DT_TABLE ) { xrtValueObjectMerge(result, item, 0); }\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( item != 0 ) { xrtValueRelease(item); item = 0; }\n", ctx->out);
    }

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* fieldType;
        const XL6_AstNode* fieldRecordType;
        XL6_AstNode* fieldClass;
        XL6_CgenFieldPersistConfig cfg;
        char fieldPath[512];

        if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
            continue;
        }
        xl6CgenCollectFieldPersistConfig(member, format, &cfg);
        if ( !xl6CgenFieldPersistIncluded(member, &cfg) ) {
            continue;
        }
        fieldType = member->children[0];
        fieldRecordType = xl6CgenClassHandleTargetType(ctx, fieldType);
        fieldClass = fieldRecordType != NULL ? xl6CgenPersistenceClassForType(ctx, fieldType) : NULL;
        xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "self", 1, member->text);
        if ( fieldClass != NULL ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts("item = ", ctx->out);
            xl6OutputPuts(fieldPath, ctx->out);
            xl6OutputPuts(" != 0 ? ", ctx->out);
            xl6CgenEmitClassToValueForFormat(ctx, fieldClass, format);
            xl6OutputPuts("((", ctx->out);
            xl6OutputPuts(xl6CgenTypeName(ctx, fieldRecordType), ctx->out);
            xl6OutputPuts("*)xl6_class_data(", ctx->out);
            xl6OutputPuts(fieldPath, ctx->out);
            xl6OutputPuts(", \"\", 0, 0)) : xrtValueNull();\n", ctx->out);
        } else if ( !xl6CgenClassArrayValueStmt(ctx, fieldType, fieldPath, "item", format) ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts("item = ", ctx->out);
            xl6CgenClassValueExpr(ctx, fieldType, fieldPath);
            xl6OutputPuts(";\n", ctx->out);
        }
        xl6CgenClassPersistSetField(ctx, member, &cfg);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("return result;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoToValueProtocol(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6CgenClassAutoToValueProto(ctx, classNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("return ", ctx->out);
    xl6CgenClassAutoToValueFormatSymbol(ctx->out, classNode, NULL);
    xl6OutputPuts("(self);\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoFromValueFormatProtocol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const char* format)
{
    const XL6_AstNode* base;
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    size_t i;

    xl6CgenClassAutoFromValueFormatProto(ctx, classNode, format);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("int ok = 1;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("xvalue* item = 0;\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("if ( self == 0 || value == 0 || xl6_xrt_value_kind(value) != XL6_XVO_DT_TABLE ) {\n", ctx->out);
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
        xl6OutputPuts("if ( !xl6_xrt_dynamic_fields_assign_value_handle(self->base.__fields, value) ) { ok = 0; }\n", ctx->out);
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
        XL6_CgenFieldPersistConfig cfg;
        char fieldPath[512];

        if ( !xl6CgenIsInstanceField(member) || member->childCount == 0 ) {
            continue;
        }
        xl6CgenCollectFieldPersistConfig(member, format, &cfg);
        if ( !xl6CgenFieldPersistIncluded(member, &cfg) ) {
            continue;
        }
        fieldType = member->children[0];
        xl6CgenClassFieldPath(fieldPath, sizeof(fieldPath), "self", 1, member->text);
        xl6CgenIndent(ctx);
        xl6OutputPuts("item = xl6_xrt_value_object_get(value, ", ctx->out);
        xl6CgenFieldPersistKeyLiteral(ctx, member, &cfg);
        xl6OutputPuts(", 0);\n", ctx->out);
        xl6CgenIndent(ctx);
        if ( xl6CgenIsNullableType(xl6CgenResolveAliasType(ctx, fieldType)) ) {
            xl6OutputPuts("if ( item != 0 ) {\n", ctx->out);
        } else {
            xl6OutputPuts("if ( item != 0 && !xl6_xrt_value_is_null(item) ) {\n", ctx->out);
        }
        ctx->indent += 1;
        xl6CgenClassAssignFromValue(ctx, fieldType, fieldPath, "item", "ok", format);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        if ( cfg.required ) {
            xl6CgenIndent(ctx);
            xl6OutputPuts("else { ok = 0; }\n", ctx->out);
        }
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("return ok;\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenClassAutoFromValueProtocol(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6CgenClassAutoFromValueProto(ctx, classNode);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("return ", ctx->out);
    xl6CgenClassAutoFromValueFormatSymbol(ctx->out, classNode, NULL);
    xl6OutputPuts("(self, value);\n", ctx->out);
    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
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
