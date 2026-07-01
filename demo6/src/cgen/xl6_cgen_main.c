static void xl6CgenEmitArgvArrayHelper(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("xvalue xvoCreateArray(void);\n", out);
    xl6OutputPuts("xvalue xvoCreateText(void* sVal, unsigned int iSize, bool bColloc);\n", out);
    xl6OutputPuts("bool xvoArrayAppendValue(xvalue pArr, xvalue pVal, bool bColloc);\n", out);
    xl6OutputPuts("void xvoUnref(xvalue pVal);\n\n", out);
    xl6OutputPuts("static xvalue xl6_argv_to_arg(int argc, char** argv)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue arg = xvoCreateArray();\n", out);
    xl6OutputPuts("    int i;\n\n", out);
    xl6OutputPuts("    if ( arg == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    for ( i = 0; i < argc; ++i ) {\n", out);
    xl6OutputPuts("        const char* text = (argv != 0 && argv[i] != 0) ? argv[i] : \"\";\n", out);
    xl6OutputPuts("        xvalue item = xvoCreateText((void*)text, 0, false);\n", out);
    xl6OutputPuts("        if ( item == 0 || !xvoArrayAppendValue(arg, item, true) ) {\n", out);
    xl6OutputPuts("            if ( item != 0 ) {\n", out);
    xl6OutputPuts("                xvoUnref(item);\n", out);
    xl6OutputPuts("            }\n", out);
    xl6OutputPuts("            xvoUnref(arg);\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return arg;\n", out);
    xl6OutputPuts("}\n\n", out);
}

static void xl6CgenEmitMainLikeWrapper(XL6_CgenContext* ctx, const char* symbolName)
{
    xl6OutputPuts("int ", ctx->out);
    xl6OutputPuts(symbolName != NULL ? symbolName : "main", ctx->out);
    xl6OutputPuts("(int argc, char** argv)\n{\n", ctx->out);
    xl6OutputPuts("    XL_InitOptions __xl6_options = xlInitOptions(argc, argv);\n", ctx->out);
    xl6OutputPuts("    xvalue __xl6_arg;\n", ctx->out);
    xl6OutputPuts("    int __xl6_code;\n", ctx->out);
    xl6OutputPuts("\n", ctx->out);
    xl6OutputPuts("    if ( !xlInit(&__xl6_options) ) {\n", ctx->out);
    xl6OutputPuts("        return 1;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    __xl6_arg = xl6_argv_to_arg(argc, argv);\n", ctx->out);
    xl6OutputPuts("    if ( __xl6_arg == 0 ) {\n", ctx->out);
    xl6OutputPuts("        xlUnit();\n", ctx->out);
    xl6OutputPuts("        return 1;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    __xl6_code = xlang_main(__xl6_arg);\n", ctx->out);
    if ( ctx->needsErrorRuntime ) {
        xl6OutputPuts("    if ( xl6_error_has() ) {\n", ctx->out);
        xl6OutputPuts("        xl6_error_print_uncaught();\n", ctx->out);
        xl6OutputPuts("        xl6_error_clear();\n", ctx->out);
        xl6OutputPuts("        xvoUnref(__xl6_arg);\n", ctx->out);
        xl6OutputPuts("        xlUnit();\n", ctx->out);
        xl6OutputPuts("        return 1;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
    }
    xl6OutputPuts("    xvoUnref(__xl6_arg);\n", ctx->out);
    xl6OutputPuts("    xlUnit();\n", ctx->out);
    xl6OutputPuts("    return __xl6_code;\n", ctx->out);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenEmitMainWrapper(XL6_CgenContext* ctx)
{
    xl6CgenEmitMainLikeWrapper(ctx, "main");
}

static void xl6CgenEmitScriptEntryWrapper(XL6_CgenContext* ctx)
{
    xl6CgenEmitMainLikeWrapper(ctx, "xl_script_entry");
}

static void xl6CgenEmitTopLevelXlangMain(XL6_CgenContext* ctx, XL6_AstNode* root, int hasErrorRuntime)
{
    const char* oldErrorExitLabel;
    size_t i;

    oldErrorExitLabel = ctx->currentErrorExitLabel;
    xl6OutputPuts("int xlang_main(xvalue arg)\n{\n", ctx->out);
    xl6OutputPuts("    (void)arg;\n", ctx->out);
    ctx->indent = 1;
    ctx->currentErrorExitLabel = "__xl6_script_error";
    for ( i = 0; i < root->childCount; ++i ) {
        if ( xl6CgenIsTopLevelRuntimeStmt(root->children[i]) ) {
            xl6CgenStmt(ctx, root->children[i]);
        }
    }
    xl6CgenCloseScope(ctx, 0);
    if ( hasErrorRuntime ) {
        xl6OutputPuts("    if ( xl6_error_has() ) {\n", ctx->out);
        xl6OutputPuts("        goto __xl6_script_error;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
    }
    xl6OutputPuts("    return 0;\n", ctx->out);
    if ( hasErrorRuntime ) {
        xl6OutputPuts("__xl6_script_error:\n", ctx->out);
        xl6OutputPuts("    xl6_error_print_uncaught();\n", ctx->out);
        xl6OutputPuts("    xl6_error_clear();\n", ctx->out);
        xl6OutputPuts("    return 1;\n", ctx->out);
    }
    xl6OutputPuts("}\n\n", ctx->out);
    ctx->indent = 0;
    ctx->currentErrorExitLabel = oldErrorExitLabel;
}
