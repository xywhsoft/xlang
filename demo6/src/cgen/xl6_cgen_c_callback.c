/*
    C 原生回调只承载能够由普通 C 函数指针完整表达的 func 签名。
    闭包环境、可选参数、xlang 可变参数和多返回值统一留在 XCall 层。
*/

static int xl6CgenCAbiCallbackValueType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    int isReturn)
{
    const char* name;

    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode == NULL || typeNode->text == NULL || typeNode->childCount != 0 ) {
        return 0;
    }
    name = typeNode->text;
    if ( isReturn && xl6CTextEquals(name, "string") ) {
        return 0;
    }
    return xl6CgenIsBoolTypeName(name) ||
           xl6CgenIsIntegerTypeName(name) ||
           xl6CgenIsFloatTypeName(name) ||
           xl6CTextEquals(name, "point") ||
           (!isReturn && xl6CTextEquals(name, "string"));
}

static int xl6CgenCAbiCallbackListsCanBridge(
    XL6_CgenContext* ctx,
    const XL6_AstNode* params,
    const XL6_AstNode* returns)
{
    size_t i;

    if ( returns != NULL && returns->childCount > 1 ) {
        return 0;
    }
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* item = params->children[i];
        const XL6_AstNode* typeNode = xl6CgenCallableItemType(item);

        if ( item == NULL || typeNode == NULL ||
             (item->flags & (XL6_AST_FLAG_BYREF | XL6_AST_FLAG_OUT)) != 0 ||
             (item->kind == XL6_AST_PARAM && item->childCount > 1) ||
             !xl6CgenCAbiCallbackValueType(ctx, typeNode, 0) ) {
            return 0;
        }
    }
    return returns == NULL || returns->childCount == 0 ||
           xl6CgenCAbiCallbackValueType(ctx, returns->children[0], 1);
}

static int xl6CgenCAbiCallbackTypeCanBridge(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    typeNode = xl6CgenSubstituteGenericType(ctx, typeNode);
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    return typeNode != NULL &&
           xl6CTextEquals(typeNode->text, "function") &&
           typeNode->childCount == 2 &&
           xl6CgenCAbiCallbackListsCanBridge(
               ctx,
               typeNode->children[0],
               typeNode->children[1]);
}

static int xl6CgenCallableDeclCanUseCAbiCallback(
    XL6_CgenContext* ctx,
    const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( func == NULL || xl6CgenFuncIsGeneric(func) ||
         (func->kind != XL6_AST_FUNC &&
          func->kind != XL6_AST_EXPORT_FUNC &&
          func->kind != XL6_AST_EXTERN_FUNC) ) {
        return 0;
    }
    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    return xl6CgenCAbiCallbackListsCanBridge(ctx, params, returns);
}

static int xl6CgenAnonCanUseCAbiCallback(
    XL6_CgenContext* ctx,
    const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( info == NULL || info->node == NULL || info->captureCount != 0 ) {
        return 0;
    }
    params = info->node->childCount > 0 ? info->node->children[0] : NULL;
    returns = info->node->childCount > 1 ? info->node->children[1] : NULL;
    return xl6CgenCAbiCallbackListsCanBridge(ctx, params, returns);
}

static void xl6CgenCallableCAbiCallbackSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* func)
{
    xl6CgenFunctionValueSymbol(ctx, func, "_cabi");
}

static void xl6CgenAnonCAbiCallbackSymbol(
    XL6_CgenContext* ctx,
    const XL6_CgenAnonFunc* info)
{
    (void)ctx;
    xl6OutputPrintf(ctx->out, "xl6_anon_%d_cabi", info != NULL ? info->id : 0);
}

static void xl6CgenCAbiCallbackTypeName(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    xl6OutputPrintf(
        ctx->out,
        "XL6_C_Callback_%016llX",
        xl6CgenTypeIdForTypeNode(ctx, typeNode));
}

static int xl6CgenRememberCAbiCallbackId(
    XL6_CgenContext* ctx,
    unsigned long long typeId,
    unsigned long long** values,
    size_t* count,
    size_t* capacity)
{
    unsigned long long* newValues;
    size_t newCapacity;
    size_t i;

    if ( ctx == NULL || values == NULL || count == NULL || capacity == NULL ) {
        return 0;
    }
    for ( i = 0; i < *count; ++i ) {
        if ( (*values)[i] == typeId ) {
            return 0;
        }
    }
    if ( *count == *capacity ) {
        if ( *capacity > SIZE_MAX / 2u ||
             (*capacity != 0 && *capacity > SIZE_MAX / sizeof(unsigned long long)) ) {
            xl6CgenError(ctx, NULL, "C ABI callback 签名表容量溢出");
            return 0;
        }
        newCapacity = *capacity == 0 ? 8 : *capacity * 2;
        if ( newCapacity > SIZE_MAX / sizeof(unsigned long long) ) {
            xl6CgenError(ctx, NULL, "C ABI callback 签名表容量溢出");
            return 0;
        }
        newValues = (unsigned long long*)xrtRealloc(
            *values,
            newCapacity * sizeof(unsigned long long));
        if ( newValues == NULL ) {
            xl6CgenError(ctx, NULL, "C ABI callback 签名表内存不足");
            return 0;
        }
        *values = newValues;
        *capacity = newCapacity;
    }
    (*values)[*count] = typeId;
    *count += 1;
    return 1;
}

static void xl6CgenEmitCAbiCallbackTypedef(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    unsigned long long typeId;
    size_t i;

    if ( !xl6CgenCAbiCallbackTypeCanBridge(ctx, typeNode) ) {
        return;
    }
    params = typeNode->children[0];
    returns = typeNode->children[1];
    typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);
    if ( !xl6CgenRememberCAbiCallbackId(
             ctx,
             typeId,
             &ctx->emittedCAbiCallbackTypes,
             &ctx->emittedCAbiCallbackTypeCount,
             &ctx->emittedCAbiCallbackTypeCapacity) ) {
        return;
    }
    xl6OutputPrintf(ctx->out, "#ifndef XL6_C_CALLBACK_%016llX\n", typeId);
    xl6OutputPrintf(ctx->out, "#define XL6_C_CALLBACK_%016llX\n", typeId);
    xl6OutputPuts("typedef ", ctx->out);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPuts(" (*", ctx->out);
    xl6CgenCAbiCallbackTypeName(ctx, typeNode);
    xl6OutputPuts(")(", ctx->out);
    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("void", ctx->out);
    } else {
        for ( i = 0; i < params->childCount; ++i ) {
            if ( i > 0 ) {
                xl6OutputPuts(", ", ctx->out);
            }
            xl6OutputPuts(
                xl6CgenCAbiTypeName(ctx, xl6CgenCallableItemType(params->children[i])),
                ctx->out);
        }
    }
    xl6OutputPuts(");\n#endif\n", ctx->out);
}

static void xl6CgenEmitCAbiCallbackTypedefsRec(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_TYPE &&
         xl6CTextEquals(node->text, "function") ) {
        xl6CgenEmitCAbiCallbackTypedef(ctx, node);
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenEmitCAbiCallbackTypedefsRec(ctx, node->children[i]);
    }
}

static void xl6CgenEmitCAbiCallbackTypeDecls(XL6_CgenContext* ctx)
{
    xl6CgenEmitCAbiCallbackTypedefsRec(ctx, ctx != NULL ? ctx->root : NULL);
}

static void xl6CgenEmitCAbiCallbackBorrowedParam(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* name)
{
    typeNode = xl6CgenResolveAliasType(ctx, typeNode);
    if ( typeNode != NULL && xl6CTextEquals(typeNode->text, "string") ) {
        xl6OutputPrintf(ctx->out, "xl6_xrt_string_dup(%s != 0 ? %s : \"\", 0u)", name, name);
    } else {
        xl6OutputPuts(name, ctx->out);
    }
}

static void xl6CgenEmitCAbiCallbackParamDecls(
    XL6_CgenContext* ctx,
    const XL6_AstNode* params)
{
    size_t i;

    if ( params == NULL || params->childCount == 0 ) {
        xl6OutputPuts("void", ctx->out);
        return;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* item = params->children[i];
        const XL6_AstNode* typeNode = xl6CgenCallableItemType(item);
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, typeNode), ctx->out);
        xl6OutputPrintf(ctx->out, " __xl6_arg_%u", (unsigned)i);
    }
}

static void xl6CgenEmitCallableCAbiCallbackAdapter(
    XL6_CgenContext* ctx,
    const XL6_AstNode* func)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    if ( !xl6CgenCallableDeclCanUseCAbiCallback(ctx, func) ||
         func->kind == XL6_AST_EXTERN_FUNC ) {
        return;
    }
    params = func->childCount > 1 ? func->children[1] : NULL;
    returns = func->childCount > 2 ? func->children[2] : NULL;
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenCallableCAbiCallbackSymbol(ctx, func);
    xl6OutputPutc('(', ctx->out);
    xl6CgenEmitCAbiCallbackParamDecls(ctx, params);
    xl6OutputPuts(")\n{\n    ", ctx->out);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("return ", ctx->out);
    }
    if ( func->kind == XL6_AST_EXPORT_FUNC ) {
        xl6CgenExportInternalSymbol(ctx, func);
    } else {
        xl6CgenFuncSymbol(ctx, func);
    }
    xl6OutputPutc('(', ctx->out);
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        {
            char argName[64];
            snprintf(argName, sizeof(argName), "__xl6_arg_%u", (unsigned)i);
            xl6CgenEmitCAbiCallbackBorrowedParam(
                ctx,
                xl6CgenCallableItemType(params->children[i]),
                argName);
        }
    }
    xl6OutputPuts(");\n}\n\n", ctx->out);
}

static void xl6CgenEmitCallableCAbiCallbackAdapters(XL6_CgenContext* ctx)
{
    size_t i;

    for ( i = 0; ctx != NULL && ctx->root != NULL && i < ctx->root->childCount; ++i ) {
        xl6CgenEmitCallableCAbiCallbackAdapter(ctx, ctx->root->children[i]);
    }
}

static void xl6CgenEmitAnonCAbiCallbackSignature(
    XL6_CgenContext* ctx,
    const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;

    if ( !xl6CgenAnonCanUseCAbiCallback(ctx, info) ) {
        return;
    }
    params = info->node->children[0];
    returns = info->node->children[1];
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenCAbiTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPutc(' ', ctx->out);
    xl6CgenAnonCAbiCallbackSymbol(ctx, info);
    xl6OutputPutc('(', ctx->out);
    xl6CgenEmitCAbiCallbackParamDecls(ctx, params);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenEmitAnonCAbiCallbackProto(
    XL6_CgenContext* ctx,
    const XL6_CgenAnonFunc* info)
{
    if ( !xl6CgenAnonCanUseCAbiCallback(ctx, info) ) {
        return;
    }
    xl6CgenEmitAnonCAbiCallbackSignature(ctx, info);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenEmitAnonCAbiCallbackProtos(XL6_CgenContext* ctx)
{
    size_t i;
    for ( i = 0; ctx != NULL && i < ctx->anonFuncCount; ++i ) {
        xl6CgenEmitAnonCAbiCallbackProto(ctx, &ctx->anonFuncs[i]);
    }
}

static void xl6CgenEmitAnonCAbiCallbackDef(
    XL6_CgenContext* ctx,
    const XL6_CgenAnonFunc* info)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    size_t i;

    if ( !xl6CgenAnonCanUseCAbiCallback(ctx, info) ) {
        return;
    }
    params = info->node->children[0];
    returns = info->node->children[1];
    xl6CgenEmitAnonCAbiCallbackSignature(ctx, info);
    xl6OutputPuts("\n{\n    ", ctx->out);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("return ", ctx->out);
    }
    xl6OutputPrintf(ctx->out, "xl6_anon_%d_invoke(0", info->id);
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        char argName[64];
        snprintf(argName, sizeof(argName), "__xl6_arg_%u", (unsigned)i);
        xl6OutputPuts(", ", ctx->out);
        xl6CgenEmitCAbiCallbackBorrowedParam(
            ctx,
            xl6CgenCallableItemType(params->children[i]),
            argName);
    }
    xl6OutputPuts(");\n}\n\n", ctx->out);
}

static void xl6CgenEmitAnonCAbiCallbackDefs(XL6_CgenContext* ctx)
{
    size_t i;
    for ( i = 0; ctx != NULL && i < ctx->anonFuncCount; ++i ) {
        xl6CgenEmitAnonCAbiCallbackDef(ctx, &ctx->anonFuncs[i]);
    }
}

static void xl6CgenEmitCAbiCallbackInvoke(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* params = typeNode->children[0];
    const XL6_AstNode* returns = typeNode->children[1];
    unsigned long long typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);
    size_t i;

    xl6OutputPuts("static ", ctx->out);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
    xl6OutputPrintf(ctx->out, " xl6_c_callback_%016llX_invoke(void* __xl6_env", typeId);
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(
            xl6CgenValueStorageCType(ctx, xl6CgenCallableItemType(params->children[i])),
            ctx->out);
        xl6OutputPrintf(ctx->out, " __xl6_arg_%u", (unsigned)i);
    }
    xl6OutputPuts(")\n{\n", ctx->out);
    xl6OutputPrintf(ctx->out,
        "    XL6_C_Callback_Box_%016llX* __xl6_box = (XL6_C_Callback_Box_%016llX*)__xl6_env;\n",
        typeId,
        typeId);
    xl6OutputPuts("    if (__xl6_box == 0 || __xl6_box->entry == 0) { xl6_function_fatal(3); }\n", ctx->out);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
        xl6OutputPuts(" __xl6_return;\n", ctx->out);
        xl6OutputPuts("    __xl6_return = ", ctx->out);
    } else {
        xl6OutputPuts("    ", ctx->out);
    }
    xl6OutputPuts("__xl6_box->entry(", ctx->out);
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        xl6OutputPrintf(ctx->out, "__xl6_arg_%u", (unsigned)i);
    }
    xl6OutputPuts(");\n", ctx->out);
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* paramType = xl6CgenResolveAliasType(
            ctx,
            xl6CgenCallableItemType(params->children[i]));
        if ( paramType != NULL && xl6CTextEquals(paramType->text, "string") ) {
            xl6OutputPrintf(ctx->out, "    xrtFree((void*)__xl6_arg_%u);\n", (unsigned)i);
        }
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    return __xl6_return;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenEmitCAbiCallbackXCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* params = typeNode->children[0];
    const XL6_AstNode* returns = typeNode->children[1];
    unsigned long long typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);
    size_t i;

    xl6OutputPrintf(ctx->out,
        "static bool xl6_c_callback_%016llX_xcall(void* __xl6_env, xl6_call_frame* __xl6_frame, xl6_call_result* __xl6_result)\n{\n",
        typeId);
    xl6OutputPrintf(ctx->out,
        "    XL6_C_Callback_Box_%016llX* __xl6_box = (XL6_C_Callback_Box_%016llX*)__xl6_env;\n",
        typeId,
        typeId);
    xl6OutputPuts("    xvalue* __xl6_value;\n", ctx->out);
    xl6OutputPrintf(ctx->out,
        "    if (__xl6_box == 0 || __xl6_box->entry == 0 || __xl6_frame == 0 || __xl6_result == 0 || __xl6_frame->Argc != %uu || (__xl6_frame->Argc > 0u && __xl6_frame->Argv == 0) || __xl6_frame->Kwargs != 0) { return false; }\n",
        (unsigned)(params != NULL ? params->childCount : 0));
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* paramType = xl6CgenCallableItemType(params->children[i]);
        const char* runtimeType = xl6CgenXValueRuntimeTypeConst(ctx, paramType);
        if ( runtimeType != NULL ) {
            xl6OutputPrintf(ctx->out,
                "    if (__xl6_frame->Argv[%u] == 0 || xl6_xrt_value_kind(__xl6_frame->Argv[%u]) != %s) { return false; }\n",
                (unsigned)i,
                (unsigned)i,
                runtimeType);
        }
    }
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(xl6CgenValueStorageCType(ctx, returns->children[0]), ctx->out);
        xl6OutputPuts(" __xl6_return = ", ctx->out);
    } else {
        xl6OutputPuts("    ", ctx->out);
    }
    xl6OutputPuts("__xl6_box->entry(", ctx->out);
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        char valueExpr[80];
        if ( i > 0 ) {
            xl6OutputPuts(", ", ctx->out);
        }
        snprintf(valueExpr, sizeof(valueExpr), "__xl6_frame->Argv[%u]", (unsigned)i);
        xl6CgenEmitVarInvokeValueExpr(
            ctx,
            xl6CgenCallableItemType(params->children[i]),
            valueExpr);
    }
    xl6OutputPuts(");\n", ctx->out);
    if ( returns != NULL && returns->childCount == 1 ) {
        xl6CgenEmitXCallSetResult(ctx, returns->children[0], 0u, "__xl6_return", 0, 0);
    }
    xl6OutputPuts("    return true;\n}\n\n", ctx->out);
}

static void xl6CgenEmitCAbiCallbackMaker(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    const XL6_AstNode* params = typeNode->children[0];
    const XL6_AstNode* returns = typeNode->children[1];
    unsigned long long typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);

    xl6OutputPrintf(ctx->out, "static XL6_Function xl6_c_callback_%016llX_make(", typeId);
    xl6CgenCAbiCallbackTypeName(ctx, typeNode);
    xl6OutputPuts(" callback)\n{\n    XL6_Function fn;\n", ctx->out);
    xl6OutputPrintf(ctx->out,
        "    XL6_C_Callback_Box_%016llX* box;\n",
        typeId);
    xl6CgenEmitFunctionSigObject(ctx, "__xl6_sig", "", typeId, params, returns);
    xl6OutputPuts("    if (callback == 0) { memset(&fn, 0, sizeof(fn)); return fn; }\n", ctx->out);
    xl6OutputPrintf(ctx->out,
        "    box = (XL6_C_Callback_Box_%016llX*)xrtMalloc(sizeof(*box));\n",
        typeId);
    xl6OutputPuts("    if (box == 0) { xl6_function_fatal(2); }\n", ctx->out);
    xl6OutputPuts("    box->ref_count = 1;\n    box->entry = callback;\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    fn.signature = 0x%016llXULL;\n", typeId);
    xl6OutputPuts("    fn.sig = &__xl6_sig;\n", ctx->out);
    xl6OutputPuts("    fn.env = box;\n", ctx->out);
    xl6OutputPuts("    fn.invoke_env = box;\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    fn.invoke = (XL6_NativeFunction)xl6_c_callback_%016llX_invoke;\n", typeId);
    xl6OutputPuts("    fn.c_entry = (XL6_NativeFunction)callback;\n", ctx->out);
    xl6OutputPuts("    fn.invoke_var = 0;\n", ctx->out);
    xl6OutputPrintf(ctx->out, "    fn.invoke_xcall = xl6_c_callback_%016llX_xcall;\n", typeId);
    xl6OutputPrintf(ctx->out, "    fn.retain = xl6_c_callback_%016llX_retain;\n", typeId);
    xl6OutputPrintf(ctx->out, "    fn.release = xl6_c_callback_%016llX_release;\n", typeId);
    xl6OutputPuts("    return fn;\n}\n\n", ctx->out);
}

static void xl6CgenEmitCAbiCallbackBox(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    unsigned long long typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);

    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n", ctx->out);
    xl6OutputPuts("void xrtFree(void* pMem);\n", ctx->out);
    xl6OutputPuts("int32_t xrtRefRetain(volatile int32_t* pCount);\n", ctx->out);
    xl6OutputPuts("int32_t xrtRefRelease(volatile int32_t* pCount);\n", ctx->out);
    xl6OutputPrintf(ctx->out,
        "typedef struct XL6_C_Callback_Box_%016llX { volatile int32_t ref_count; ",
        typeId);
    xl6CgenCAbiCallbackTypeName(ctx, typeNode);
    xl6OutputPrintf(ctx->out, " entry; } XL6_C_Callback_Box_%016llX;\n", typeId);
    xl6OutputPrintf(ctx->out,
        "static void xl6_c_callback_%016llX_retain(void* env)\n{\n"
        "    XL6_C_Callback_Box_%016llX* box = (XL6_C_Callback_Box_%016llX*)env;\n"
        "    if (box != 0 && xrtRefRetain(&box->ref_count) < 0) { xl6_function_fatal(3); }\n"
        "}\n\n",
        typeId,
        typeId,
        typeId);
    xl6OutputPrintf(ctx->out,
        "static void xl6_c_callback_%016llX_release(void* env)\n{\n"
        "    XL6_C_Callback_Box_%016llX* box = (XL6_C_Callback_Box_%016llX*)env;\n"
        "    int32_t remaining;\n"
        "    if (box == 0) { return; }\n"
        "    remaining = xrtRefRelease(&box->ref_count);\n"
        "    if (remaining < 0) { xl6_function_fatal(3); }\n"
        "    if (remaining == 0) { xrtFree(box); }\n"
        "}\n\n",
        typeId,
        typeId,
        typeId);
}

static void xl6CgenEmitCAbiCallbackBridge(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    unsigned long long typeId;

    if ( !xl6CgenCAbiCallbackTypeCanBridge(ctx, typeNode) ) {
        return;
    }
    typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);
    if ( !xl6CgenRememberCAbiCallbackId(
             ctx,
             typeId,
             &ctx->emittedCAbiCallbackBridges,
             &ctx->emittedCAbiCallbackBridgeCount,
             &ctx->emittedCAbiCallbackBridgeCapacity) ) {
        return;
    }
    xl6OutputPrintf(ctx->out, "#ifndef XL6_C_CALLBACK_BRIDGE_%016llX\n", typeId);
    xl6OutputPrintf(ctx->out, "#define XL6_C_CALLBACK_BRIDGE_%016llX\n", typeId);
    xl6CgenEmitCAbiCallbackBox(ctx, typeNode);
    xl6CgenEmitCAbiCallbackInvoke(ctx, typeNode);
    xl6CgenEmitCAbiCallbackXCall(ctx, typeNode);
    xl6CgenEmitCAbiCallbackMaker(ctx, typeNode);
    xl6OutputPuts("#endif\n", ctx->out);
}

static void xl6CgenEmitCAbiCallbackBridgesRec(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    size_t i;
    if ( node == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CTextEquals(node->text, "function") ) {
        xl6CgenEmitCAbiCallbackBridge(ctx, node);
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenEmitCAbiCallbackBridgesRec(ctx, node->children[i]);
    }
}

static void xl6CgenEmitCAbiCallbackBridges(XL6_CgenContext* ctx)
{
    xl6CgenEmitCAbiCallbackBridgesRec(ctx, ctx != NULL ? ctx->root : NULL);
}

static void xl6CgenEmitCAbiCallbackMakeExpr(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* valueName)
{
    unsigned long long typeId = xl6CgenTypeIdForTypeNode(ctx, typeNode);
    xl6OutputPrintf(
        ctx->out,
        "xl6_c_callback_%016llX_make(%s)",
        typeId,
        valueName != NULL ? valueName : "0");
}

static int xl6CgenEmitCAbiCallbackArgument(
    XL6_CgenContext* ctx,
    const XL6_AstNode* functionType,
    const XL6_AstNode* expr)
{
    XL6_CgenAnonFunc* anonInfo;
    unsigned long long typeId;

    if ( !xl6CgenCAbiCallbackTypeCanBridge(ctx, functionType) ) {
        return 0;
    }
    anonInfo = expr != NULL && expr->kind == XL6_AST_ANON_FUNC ?
        xl6CgenFindAnonFuncInfo(ctx, expr) :
        NULL;
    if ( anonInfo != NULL && anonInfo->captureCount > 0 ) {
        xl6CgenError(
            ctx,
            expr,
            "捕获闭包不能转换为无 userdata 的 C 回调；请使用非捕获函数，或让 C API 显式接收 userdata");
        xl6OutputPutc('(', ctx->out);
        xl6CgenCAbiCallbackTypeName(ctx, functionType);
        xl6OutputPuts(")0", ctx->out);
        return 1;
    }
    typeId = xl6CgenTypeIdForTypeNode(ctx, functionType);
    xl6OutputPutc('(', ctx->out);
    xl6CgenCAbiCallbackTypeName(ctx, functionType);
    xl6OutputPuts(")xl6_function_take_c_entry(", ctx->out);
    xl6CgenFunctionOwnedValueExpr(ctx, expr, functionType);
    xl6OutputPrintf(ctx->out, ", 0x%016llXULL)", typeId);
    return 1;
}
