static void xl6CgenEmitArgvArrayHelper(XL6_Output* out)
{
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

static int xl6CgenMainReturnsInt(const XL6_AstNode* mainFunc)
{
    const XL6_AstNode* returns;

    returns = mainFunc != NULL && mainFunc->childCount > 2 ? mainFunc->children[2] : NULL;
    return returns != NULL && returns->childCount == 1;
}

static void xl6CgenEmitMainWrapper(XL6_CgenContext* ctx, const XL6_AstNode* mainFunc)
{
    int returnsInt;

    returnsInt = xl6CgenMainReturnsInt(mainFunc);
    xl6OutputPuts("int main(int argc, char** argv)\n{\n", ctx->out);
    xl6OutputPuts("    XL_InitOptions __xl6_options = xlInitOptions(argc, argv);\n", ctx->out);
    xl6OutputPuts("    xvalue __xl6_arg;\n", ctx->out);
    if ( returnsInt ) {
        xl6OutputPuts("    int __xl6_code;\n", ctx->out);
    }
    xl6OutputPuts("\n", ctx->out);
    xl6OutputPuts("    if ( !xlInit(&__xl6_options) ) {\n", ctx->out);
    xl6OutputPuts("        return 1;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    xl6OutputPuts("    __xl6_arg = xl6_argv_to_arg(argc, argv);\n", ctx->out);
    xl6OutputPuts("    if ( __xl6_arg == 0 ) {\n", ctx->out);
    xl6OutputPuts("        xlUnit();\n", ctx->out);
    xl6OutputPuts("        return 1;\n", ctx->out);
    xl6OutputPuts("    }\n", ctx->out);
    if ( returnsInt ) {
        xl6OutputPuts("    __xl6_code = (int)xl_user_main(__xl6_arg);\n", ctx->out);
        if ( ctx->needsErrorRuntime ) {
            xl6OutputPuts("    if ( xl6_error_has() ) {\n", ctx->out);
            xl6OutputPuts("        xl6_error_clear();\n", ctx->out);
            xl6OutputPuts("        xlUnit();\n", ctx->out);
            xl6OutputPuts("        return 1;\n", ctx->out);
            xl6OutputPuts("    }\n", ctx->out);
        }
        xl6OutputPuts("    xlUnit();\n", ctx->out);
        xl6OutputPuts("    return __xl6_code;\n", ctx->out);
    } else {
        xl6OutputPuts("    xl_user_main(__xl6_arg);\n", ctx->out);
        if ( ctx->needsErrorRuntime ) {
            xl6OutputPuts("    if ( xl6_error_has() ) {\n", ctx->out);
            xl6OutputPuts("        xl6_error_clear();\n", ctx->out);
            xl6OutputPuts("        xlUnit();\n", ctx->out);
            xl6OutputPuts("        return 1;\n", ctx->out);
            xl6OutputPuts("    }\n", ctx->out);
        }
        xl6OutputPuts("    xlUnit();\n", ctx->out);
        xl6OutputPuts("    return 0;\n", ctx->out);
    }
    xl6OutputPuts("}\n", ctx->out);
}
