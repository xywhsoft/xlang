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
    xl6OutputPuts("    __xl6_arg = xl6_command_argv_to_arg(argc, argv);\n", ctx->out);
    xl6OutputPuts("    if ( __xl6_arg == 0 ) {\n", ctx->out);
    xl6OutputPuts("        xlUnit();\n", ctx->out);
    xl6OutputPuts("        return 1;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    __xl6_code = xlang_main(__xl6_arg);\n", ctx->out);
    if ( ctx->needsErrorRuntime ) {
        xl6OutputPuts("    if ( xl6_error_has() ) {\n", ctx->out);
        xl6OutputPuts("        xl6_error_print_uncaught();\n", ctx->out);
        xl6OutputPuts("        xl6_error_clear();\n", ctx->out);
        xl6OutputPuts("        xl6_error_tls_unit();\n", ctx->out);
        xl6OutputPuts("        xvoUnref(__xl6_arg);\n", ctx->out);
        xl6OutputPuts("        xlUnit();\n", ctx->out);
        xl6OutputPuts("        return 1;\n", ctx->out);
        xl6OutputPuts("    }\n", ctx->out);
    }
    xl6OutputPuts("    xvoUnref(__xl6_arg);\n", ctx->out);
    if ( ctx->needsErrorRuntime ) {
        xl6OutputPuts("    xl6_error_tls_unit();\n", ctx->out);
    }
    xl6OutputPuts("    xlUnit();\n", ctx->out);
    xl6OutputPuts("    return __xl6_code;\n", ctx->out);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenEmitMainWrapper(XL6_CgenContext* ctx)
{
    xl6CgenEmitMainLikeWrapper(ctx, "main");
}

static void xl6CgenEmitScriptErrorBridge(XL6_CgenContext* ctx)
{
    if ( !ctx->needsErrorRuntime ) {
        return;
    }

    xl6OutputPuts("int xl_script_has_error(void)\n{\n", ctx->out);
    xl6OutputPuts("    return xl6_error_has();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
    xl6OutputPuts("void xl_script_print_uncaught(void)\n{\n", ctx->out);
    xl6OutputPuts("    xl6_error_print_uncaught();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
    xl6OutputPuts("void xl_script_clear_error(void)\n{\n", ctx->out);
    xl6OutputPuts("    xl6_error_clear();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
    xl6OutputPuts("void xl_script_error_unit(void)\n{\n", ctx->out);
    xl6OutputPuts("    xl6_error_clear();\n", ctx->out);
    xl6OutputPuts("    xl6_error_tls_unit();\n", ctx->out);
    xl6OutputPuts("}\n\n", ctx->out);
}
