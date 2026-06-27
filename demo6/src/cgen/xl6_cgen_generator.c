static void xl6CgenEmitGeneratorRuntime(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_GENERATOR_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_GENERATOR_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xcoro_struct* xcoro;\n", out);
    xl6OutputPuts("typedef void (*xl6_xco_entry)(void* pParam);\n", out);
    xl6OutputPuts("typedef struct XL6_Generator {\n", out);
    xl6OutputPuts("    xcoro co;\n", out);
    xl6OutputPuts("    void* frame;\n", out);
    xl6OutputPuts("    void* value;\n", out);
    xl6OutputPuts("    XL6_Error* error;\n", out);
    xl6OutputPuts("    size_t valueSize;\n", out);
    xl6OutputPuts("    int hasValue;\n", out);
    xl6OutputPuts("    int done;\n", out);
    xl6OutputPuts("} XL6_Generator;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("#ifndef XRT_CO_DEAD\n", out);
    xl6OutputPuts("#define XRT_CO_DEAD 3\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("xcoro xrtCoCreate(xl6_xco_entry pfnEntry, void* pParam, size_t iStackSize);\n", out);
    xl6OutputPuts("void xrtCoDestroy(xcoro pCo);\n", out);
    xl6OutputPuts("bool xrtCoResume(xcoro pCo);\n", out);
    xl6OutputPuts("void xrtCoYield(void);\n", out);
    xl6OutputPuts("int xrtCoGetState(xcoro pCo);\n\n", out);

    xl6OutputPuts("static XL6_Generator* xl6_generator_create(size_t valueSize, void* frame, xl6_xco_entry entry)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    XL6_Generator* gen;\n", out);
    xl6OutputPuts("    if ( valueSize == 0 || entry == 0 ) { xrtFree(frame); return 0; }\n", out);
    xl6OutputPuts("    gen = (XL6_Generator*)xrtMalloc(sizeof(XL6_Generator));\n", out);
    xl6OutputPuts("    if ( gen == 0 ) { xrtFree(frame); return 0; }\n", out);
    xl6OutputPuts("    memset(gen, 0, sizeof(XL6_Generator));\n", out);
    xl6OutputPuts("    gen->value = xrtMalloc(valueSize);\n", out);
    xl6OutputPuts("    if ( gen->value == 0 ) { xrtFree(frame); xrtFree(gen); return 0; }\n", out);
    xl6OutputPuts("    memset(gen->value, 0, valueSize);\n", out);
    xl6OutputPuts("    gen->valueSize = valueSize;\n", out);
    xl6OutputPuts("    gen->frame = frame;\n", out);
    xl6OutputPuts("    gen->co = xrtCoCreate(entry, gen, 0);\n", out);
    xl6OutputPuts("    if ( gen->co == 0 ) { xrtFree(gen->value); xrtFree(frame); xrtFree(gen); return 0; }\n", out);
    xl6OutputPuts("    return gen;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_generator_store_active_error(XL6_Generator* gen)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( gen == 0 || !xl6_error_has() ) { return; }\n", out);
    xl6OutputPuts("    if ( gen->error != 0 ) { xl6_error_free(gen->error); gen->error = 0; }\n", out);
    xl6OutputPuts("    gen->error = xl6_error_take();\n", out);
    xl6OutputPuts("    gen->done = 1;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_generator_raise_stored_error(XL6_Generator* gen)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( gen == 0 || gen->error == 0 ) { return false; }\n", out);
    xl6OutputPuts("    xl6_error_reraise(gen->error);\n", out);
    xl6OutputPuts("    return true;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_generator_destroy(XL6_Generator* gen)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( gen == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( gen->co != 0 ) { xrtCoDestroy(gen->co); gen->co = 0; }\n", out);
    xl6OutputPuts("    if ( gen->value != 0 ) { xrtFree(gen->value); gen->value = 0; }\n", out);
    xl6OutputPuts("    if ( gen->error != 0 ) { xl6_error_free(gen->error); gen->error = 0; }\n", out);
    xl6OutputPuts("    if ( gen->frame != 0 ) { xrtFree(gen->frame); gen->frame = 0; }\n", out);
    xl6OutputPuts("    xrtFree(gen);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_generator_frame(XL6_Generator* gen)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return gen != 0 ? gen->frame : 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_generator_yield_copy(XL6_Generator* gen, const void* value, size_t valueSize, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( gen == 0 || value == 0 || gen->value == 0 || gen->valueSize != valueSize ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"GeneratorError\", \"invalid generator yield\", file, line, column);\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(gen->value, value, valueSize);\n", out);
    xl6OutputPuts("    gen->hasValue = 1;\n", out);
    xl6OutputPuts("    xrtCoYield();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static bool xl6_generator_next(XL6_Generator* gen, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( gen == 0 ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"GeneratorError\", \"iterate null generator\", file, line, column);\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( xl6_generator_raise_stored_error(gen) ) { return false; }\n", out);
    xl6OutputPuts("    if ( gen->done || gen->co == 0 ) { return false; }\n", out);
    xl6OutputPuts("    gen->hasValue = 0;\n", out);
    xl6OutputPuts("    if ( !xrtCoResume(gen->co) ) {\n", out);
    xl6OutputPuts("        gen->done = 1;\n", out);
    xl6OutputPuts("        if ( xl6_error_has() ) { xl6_generator_store_active_error(gen); xl6_generator_raise_stored_error(gen); }\n", out);
    xl6OutputPuts("        return false;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( xl6_error_has() ) { xl6_generator_store_active_error(gen); xl6_generator_raise_stored_error(gen); return false; }\n", out);
    xl6OutputPuts("    if ( xl6_generator_raise_stored_error(gen) ) { return false; }\n", out);
    xl6OutputPuts("    if ( gen->hasValue ) { return true; }\n", out);
    xl6OutputPuts("    if ( xrtCoGetState(gen->co) == XRT_CO_DEAD ) { gen->done = 1; }\n", out);
    xl6OutputPuts("    return false;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_generator_value_into(XL6_Generator* gen, void* outValue, size_t valueSize, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( xl6_generator_raise_stored_error(gen) ) { return; }\n", out);
    xl6OutputPuts("    if ( gen == 0 || outValue == 0 || gen->value == 0 || !gen->hasValue || gen->valueSize != valueSize ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"GeneratorError\", \"invalid generator value\", file, line, column);\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(outValue, gen->value, valueSize);\n", out);
    xl6OutputPuts("}\n\n", out);
}

static int xl6CgenFuncReturnsGenerator(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* returns;

    returns = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    return returns != NULL &&
           returns->childCount == 1 &&
           xl6CgenIsGeneratorType(xl6CgenResolveAliasType(ctx, returns->children[0]));
}

static XL6_AstNode* xl6CgenCloneGeneratorItemType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6CgenCloneSingleGenericArgType(ctx, typeNode, "generator");
}

static const XL6_AstNode* xl6CgenGeneratorReturnType(const XL6_AstNode* node)
{
    const XL6_AstNode* returns;

    returns = node != NULL && node->childCount > 2 ? node->children[2] : NULL;
    return returns != NULL && returns->childCount == 1 ? returns->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenGeneratorParams(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6CgenGeneratorBody(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 3 ? node->children[3] : NULL;
}

static void xl6CgenGeneratorWorkerSymbol(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6OutputPuts("xl_gen_worker_", ctx->out);
    xl6CgenFuncSymbol(ctx, node);
}

static void xl6CgenGeneratorFrameName(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    xl6OutputPuts("XL6_GenFrame_", ctx->out);
    xl6CgenFuncSymbol(ctx, node);
}

static void xl6CgenGeneratorFrameDecl(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* params = xl6CgenGeneratorParams(node);
    size_t i;

    if ( params == NULL || params->childCount == 0 ) {
        return;
    }

    xl6OutputPuts("typedef struct ", ctx->out);
    xl6CgenGeneratorFrameName(ctx, node);
    xl6OutputPuts(" {\n", ctx->out);
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        xl6OutputPuts("    ", ctx->out);
        xl6OutputPuts(param != NULL && param->childCount > 0 ? xl6CgenTypeName(ctx, param->children[0]) : "int", ctx->out);
        xl6OutputPutc(' ', ctx->out);
        xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6OutputPuts("} ", ctx->out);
    xl6CgenGeneratorFrameName(ctx, node);
    xl6OutputPuts(";\n\n", ctx->out);
}

static void xl6CgenGeneratorWorker(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* itemType)
{
    const XL6_AstNode* params = xl6CgenGeneratorParams(node);
    const XL6_AstNode* body = xl6CgenGeneratorBody(node);
    const XL6_AstNode* prevReturnType;
    const XL6_AstNode* prevReturnParams;
    const XL6_AstNode* prevReturns;
    const XL6_AstNode* prevReturnFunc;
    const XL6_AstNode* prevGeneratorItemType;
    const char* prevGeneratorHandleName;
    const char* prevErrorExitLabel;
    size_t varMark;
    size_t cleanupMark;
    size_t i;

    xl6OutputPuts("static void ", ctx->out);
    xl6CgenGeneratorWorkerSymbol(ctx, node);
    xl6OutputPuts("(void* __xl6_arg)\n{\n", ctx->out);
    ctx->indent += 1;

    varMark = ctx->varCount;
    cleanupMark = ctx->cleanupCount;
    prevReturnType = ctx->currentReturnType;
    prevReturnParams = ctx->currentReturnParams;
    prevReturns = ctx->currentReturns;
    prevReturnFunc = ctx->currentReturnFunc;
    prevGeneratorItemType = ctx->currentGeneratorItemType;
    prevGeneratorHandleName = ctx->currentGeneratorHandleName;
    prevErrorExitLabel = ctx->currentErrorExitLabel;

    xl6CgenIndent(ctx);
    xl6OutputPuts("XL6_Generator* __xl6_gen = (XL6_Generator*)__xl6_arg;\n", ctx->out);
    if ( params != NULL && params->childCount > 0 ) {
        xl6CgenIndent(ctx);
        xl6CgenGeneratorFrameName(ctx, node);
        xl6OutputPuts("* __xl6_frame = (", ctx->out);
        xl6CgenGeneratorFrameName(ctx, node);
        xl6OutputPuts("*)xl6_generator_frame(__xl6_gen);\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( __xl6_frame == 0 ) { return; }\n", ctx->out);
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];

            xl6CgenIndent(ctx);
            xl6OutputPuts(param != NULL && param->childCount > 0 ? xl6CgenTypeName(ctx, param->children[0]) : "int", ctx->out);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
            xl6OutputPuts(" = __xl6_frame->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
            xl6OutputPuts(";\n", ctx->out);
        }
    }

    ctx->currentReturnType = xl6CgenGeneratorReturnType(node);
    ctx->currentReturnParams = params;
    ctx->currentReturns = node->childCount > 2 ? node->children[2] : NULL;
    ctx->currentReturnFunc = node;
    ctx->currentGeneratorItemType = itemType;
    ctx->currentGeneratorHandleName = "__xl6_gen";
    ctx->currentErrorExitLabel = "__xl6_generator_error";
    xl6CgenRegisterParamTypes(ctx, params);
    if ( body != NULL ) {
        xl6CgenBlock(ctx, body);
    }
    if ( !xl6CgenBlockEndsWithReturn(body) ) {
        xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    }

    ctx->indent -= 1;
    xl6OutputPuts("__xl6_generator_error:\n", ctx->out);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_generator_store_active_error(__xl6_gen);\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPuts("return;\n", ctx->out);

    ctx->currentReturnType = prevReturnType;
    ctx->currentReturnParams = prevReturnParams;
    ctx->currentReturns = prevReturns;
    ctx->currentReturnFunc = prevReturnFunc;
    ctx->currentGeneratorItemType = prevGeneratorItemType;
    ctx->currentGeneratorHandleName = prevGeneratorHandleName;
    ctx->currentErrorExitLabel = prevErrorExitLabel;
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeVarsSince(ctx, varMark);

    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
}

static void xl6CgenGeneratorFunc(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* params = xl6CgenGeneratorParams(node);
    const XL6_AstNode* returnType = xl6CgenGeneratorReturnType(node);
    XL6_AstNode* itemType = xl6CgenCloneGeneratorItemType(ctx, returnType);
    const char* itemCType;
    size_t i;

    if ( itemType == NULL ) {
        xl6CgenError(ctx, returnType, "generator 必须写成 generator<T>");
        itemType = xl6CgenNewSimpleType("int", node);
    }
    itemCType = xl6CgenTypeName(ctx, itemType);

    xl6CgenGeneratorFrameDecl(ctx, node);
    xl6CgenGeneratorWorker(ctx, node, itemType);

    xl6CgenFuncProto(ctx, node);
    xl6OutputPuts("\n{\n", ctx->out);
    ctx->indent += 1;

    if ( params != NULL && params->childCount > 0 ) {
        xl6CgenIndent(ctx);
        xl6CgenGeneratorFrameName(ctx, node);
        xl6OutputPuts("* __xl6_frame = (", ctx->out);
        xl6CgenGeneratorFrameName(ctx, node);
        xl6OutputPuts("*)xrtMalloc(sizeof(", ctx->out);
        xl6CgenGeneratorFrameName(ctx, node);
        xl6OutputPuts("));\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("if ( __xl6_frame == 0 ) { return 0; }\n", ctx->out);
        for ( i = 0; i < params->childCount; ++i ) {
            const XL6_AstNode* param = params->children[i];

            xl6CgenIndent(ctx);
            xl6OutputPuts("__xl6_frame->", ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
            xl6OutputPuts(" = ", ctx->out);
            xl6CgenFputsCIdent(ctx->out, param != NULL ? param->text : NULL);
            xl6OutputPuts(";\n", ctx->out);
        }
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("return xl6_generator_create(sizeof(", ctx->out);
    xl6OutputPuts(itemCType, ctx->out);
    xl6OutputPuts("), ", ctx->out);
    if ( params != NULL && params->childCount > 0 ) {
        xl6OutputPuts("__xl6_frame", ctx->out);
    } else {
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPuts(", ", ctx->out);
    xl6CgenGeneratorWorkerSymbol(ctx, node);
    xl6OutputPuts(");\n", ctx->out);

    ctx->indent -= 1;
    xl6OutputPuts("}\n\n", ctx->out);
    xl6AstFree(itemType);
}

static void xl6CgenYieldStmt(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* itemType;
    const XL6_AstNode* expr;
    const char* cType;

    if ( ctx == NULL || ctx->currentGeneratorHandleName == NULL || ctx->currentGeneratorItemType == NULL ) {
        xl6CgenError(ctx, node, "yield 必须位于 generator 函数体内");
        return;
    }
    if ( node == NULL || node->childCount == 0 ) {
        xl6CgenError(ctx, node, "yield 需要表达式");
        return;
    }

    itemType = ctx->currentGeneratorItemType;
    expr = node->children[0];
    cType = xl6CgenTypeName(ctx, itemType);

    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_generator_yield_copy(", ctx->out);
    xl6OutputPuts(ctx->currentGeneratorHandleName, ctx->out);
    xl6OutputPuts(", &(", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPuts("){", ctx->out);
    xl6CgenExpr(ctx, expr);
    xl6OutputPuts("}, sizeof(", ctx->out);
    xl6OutputPuts(cType, ctx->out);
    xl6OutputPrintf(ctx->out, "), \"\", %d, %d);\n", node->line, node->column);
    xl6CgenEmitExceptionPropagate(ctx, node);
}
