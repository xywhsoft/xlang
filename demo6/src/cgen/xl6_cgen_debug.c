static void xl6CgenEmitDebugRuntime(XL6_Output* out)
{
    xl6OutputPuts("typedef struct XL6_DebugState {\n", out);
    xl6OutputPuts("    int inited;\n", out);
    xl6OutputPuts("    int stopOnEntry;\n", out);
    xl6OutputPuts("    int stoppedOnce;\n", out);
    xl6OutputPuts("    int stepNext;\n", out);
    xl6OutputPuts("    const char* breakpoints;\n", out);
    xl6OutputPuts("    const char* commands;\n", out);
    xl6OutputPuts("    size_t commandPos;\n", out);
    xl6OutputPuts("} XL6_DebugState;\n\n", out);

    xl6OutputPuts("static XL6_DebugState xl6_debug_state;\n\n", out);

    xl6OutputPuts("static int xl6_debug_env_enabled(const char* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return value != 0 && value[0] != '\\0' && strcmp(value, \"0\") != 0 && strcmp(value, \"false\") != 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_debug_init(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( xl6_debug_state.inited ) { return; }\n", out);
    xl6OutputPuts("    xl6_debug_state.inited = 1;\n", out);
    xl6OutputPuts("    xl6_debug_state.stopOnEntry = xl6_debug_env_enabled(getenv(\"XL6_DBG_STOP_ON_ENTRY\"));\n", out);
    xl6OutputPuts("    xl6_debug_state.breakpoints = getenv(\"XL6_DBG_BREAKPOINTS\");\n", out);
    xl6OutputPuts("    xl6_debug_state.commands = getenv(\"XL6_DBG_COMMANDS\");\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_debug_token_matches(const char* tokenStart, size_t tokenLen, const char* file, int line)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* colon = 0;\n", out);
    xl6OutputPuts("    char* endPtr = 0;\n", out);
    xl6OutputPuts("    long value;\n", out);
    xl6OutputPuts("    size_t i;\n", out);
    xl6OutputPuts("    if ( tokenStart == 0 || tokenLen == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    for ( i = 0; i < tokenLen; ++i ) {\n", out);
    xl6OutputPuts("        if ( tokenStart[i] == ':' ) { colon = tokenStart + i; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( colon != 0 ) {\n", out);
    xl6OutputPuts("        size_t fileLen = (size_t)(colon - tokenStart);\n", out);
    xl6OutputPuts("        const char* lineText = colon + 1;\n", out);
    xl6OutputPuts("        if ( file == 0 || strncmp(tokenStart, file, fileLen) != 0 || file[fileLen] != '\\0' ) { return 0; }\n", out);
    xl6OutputPuts("        value = strtol(lineText, &endPtr, 10);\n", out);
    xl6OutputPuts("        return endPtr != lineText && value == line;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    value = strtol(tokenStart, &endPtr, 10);\n", out);
    xl6OutputPuts("    return endPtr != tokenStart && value == line;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_debug_has_breakpoint(const char* file, int line)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* text;\n", out);
    xl6OutputPuts("    const char* start;\n", out);
    xl6OutputPuts("    const char* p;\n", out);
    xl6OutputPuts("    xl6_debug_init();\n", out);
    xl6OutputPuts("    text = xl6_debug_state.breakpoints;\n", out);
    xl6OutputPuts("    if ( text == 0 || text[0] == '\\0' ) { return 0; }\n", out);
    xl6OutputPuts("    start = text;\n", out);
    xl6OutputPuts("    for ( p = text; ; ++p ) {\n", out);
    xl6OutputPuts("        if ( *p == ',' || *p == ';' || *p == '\\n' || *p == '\\0' ) {\n", out);
    xl6OutputPuts("            while ( start < p && (*start == ' ' || *start == '\\t') ) { ++start; }\n", out);
    xl6OutputPuts("            if ( xl6_debug_token_matches(start, (size_t)(p - start), file, line) ) { return 1; }\n", out);
    xl6OutputPuts("            if ( *p == '\\0' ) { break; }\n", out);
    xl6OutputPuts("            start = p + 1;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_debug_next_command(char* outCommand, size_t outSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* text;\n", out);
    xl6OutputPuts("    size_t pos;\n", out);
    xl6OutputPuts("    size_t used;\n", out);
    xl6OutputPuts("    xl6_debug_init();\n", out);
    xl6OutputPuts("    if ( outCommand == 0 || outSize == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    outCommand[0] = '\\0';\n", out);
    xl6OutputPuts("    text = xl6_debug_state.commands;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    pos = xl6_debug_state.commandPos;\n", out);
    xl6OutputPuts("    while ( text[pos] == '\\r' || text[pos] == '\\n' || text[pos] == ';' || text[pos] == ' ' || text[pos] == '\\t' ) { ++pos; }\n", out);
    xl6OutputPuts("    if ( text[pos] == '\\0' ) { xl6_debug_state.commandPos = pos; return 0; }\n", out);
    xl6OutputPuts("    used = 0;\n", out);
    xl6OutputPuts("    while ( text[pos] != '\\0' && text[pos] != '\\r' && text[pos] != '\\n' && text[pos] != ';' ) {\n", out);
    xl6OutputPuts("        if ( used + 1 < outSize ) { outCommand[used++] = text[pos]; }\n", out);
    xl6OutputPuts("        ++pos;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    while ( used > 0 && (outCommand[used - 1] == ' ' || outCommand[used - 1] == '\\t') ) { --used; }\n", out);
    xl6OutputPuts("    outCommand[used] = '\\0';\n", out);
    xl6OutputPuts("    xl6_debug_state.commandPos = pos;\n", out);
    xl6OutputPuts("    return used > 0;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void xl6_debug_probe(const char* file, int line, int column, const char* kind, const char* functionName)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char command[64];\n", out);
    xl6OutputPuts("    const char* reason;\n", out);
    xl6OutputPuts("    int shouldStop;\n", out);
    xl6OutputPuts("    xl6_debug_init();\n", out);
    xl6OutputPuts("    shouldStop = 0;\n", out);
    xl6OutputPuts("    reason = \"step\";\n", out);
    xl6OutputPuts("    if ( xl6_debug_state.stopOnEntry && !xl6_debug_state.stoppedOnce ) { shouldStop = 1; reason = \"entry\"; }\n", out);
    xl6OutputPuts("    if ( xl6_debug_state.stepNext ) { shouldStop = 1; reason = \"step\"; }\n", out);
    xl6OutputPuts("    if ( xl6_debug_has_breakpoint(file, line) ) { shouldStop = 1; reason = \"breakpoint\"; }\n", out);
    xl6OutputPuts("    if ( !shouldStop ) { return; }\n", out);
    xl6OutputPuts("    xl6_debug_state.stoppedOnce = 1;\n", out);
    xl6OutputPuts("    xl6_debug_state.stepNext = 0;\n", out);
    xl6OutputPuts("    print", out);
    xl6OutputPuts("f(\"xl-debug stopped reason=%s file=%s line=%d column=%d function=%s kind=%s\\n\", reason, file != 0 ? file : \"<unknown>\", line, column, functionName != 0 ? functionName : \"<top-level>\", kind != 0 ? kind : \"statement\");\n", out);
    xl6OutputPuts("    ff", out);
    xl6OutputPuts("lush(std", out);
    xl6OutputPuts("out);\n", out);
    xl6OutputPuts("    while ( xl6_debug_next_command(command, sizeof(command)) ) {\n", out);
    xl6OutputPuts("        if ( strcmp(command, \"stack\") == 0 || strcmp(command, \"bt\") == 0 ) {\n", out);
    xl6OutputPuts("            print", out);
    xl6OutputPuts("f(\"xl-debug frame #0 function=%s file=%s line=%d column=%d\\n\", functionName != 0 ? functionName : \"<top-level>\", file != 0 ? file : \"<unknown>\", line, column);\n", out);
    xl6OutputPuts("        } else if ( strcmp(command, \"scopes\") == 0 ) {\n", out);
    xl6OutputPuts("            print", out);
    xl6OutputPuts("f(\"xl-debug scope locals available=false reason=rc0-probe\\n\");\n", out);
    xl6OutputPuts("        } else if ( strcmp(command, \"variables\") == 0 || strcmp(command, \"vars\") == 0 ) {\n", out);
    xl6OutputPuts("            print", out);
    xl6OutputPuts("f(\"xl-debug variables function=%s kind=%s line=%d\\n\", functionName != 0 ? functionName : \"<top-level>\", kind != 0 ? kind : \"statement\", line);\n", out);
    xl6OutputPuts("        } else if ( strcmp(command, \"next\") == 0 || strcmp(command, \"step\") == 0 || strcmp(command, \"s\") == 0 ) {\n", out);
    xl6OutputPuts("            xl6_debug_state.stepNext = 1;\n", out);
    xl6OutputPuts("            break;\n", out);
    xl6OutputPuts("        } else if ( strcmp(command, \"continue\") == 0 || strcmp(command, \"c\") == 0 ) {\n", out);
    xl6OutputPuts("            break;\n", out);
    xl6OutputPuts("        } else if ( strcmp(command, \"quit\") == 0 || strcmp(command, \"q\") == 0 ) {\n", out);
    xl6OutputPuts("            exit(0);\n", out);
    xl6OutputPuts("        } else {\n", out);
    xl6OutputPuts("            print", out);
    xl6OutputPuts("f(\"xl-debug unknown-command command=%s\\n\", command);\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        ff", out);
    xl6OutputPuts("lush(std", out);
    xl6OutputPuts("out);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);
}

static const char* xl6CgenDebugStmtKind(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return "statement";
    }
    switch ( node->kind ) {
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
    case XL6_AST_MULTI_VAR_DECL:
        return "decl";
    case XL6_AST_ASSIGN:
    case XL6_AST_MULTI_ASSIGN:
        return "assign";
    case XL6_AST_RETURN:
        return "return";
    case XL6_AST_EXPR_STMT:
        return "expr";
    case XL6_AST_IF:
        return "if";
    case XL6_AST_WHILE:
    case XL6_AST_DO_WHILE:
    case XL6_AST_FOR:
        return "loop";
    case XL6_AST_THROW:
        return "throw";
    case XL6_AST_ASSERT:
        return "assert";
    case XL6_AST_TRY:
        return "try";
    case XL6_AST_BREAK:
        return "break";
    case XL6_AST_CONTINUE:
        return "continue";
    case XL6_AST_YIELD:
        return "yield";
    default:
        return "statement";
    }
}

static int xl6CgenDebugNodeCanProbe(const XL6_AstNode* node)
{
    if ( node == NULL || node->line <= 0 ) {
        return 0;
    }
    switch ( node->kind ) {
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
    case XL6_AST_MULTI_VAR_DECL:
    case XL6_AST_ASSIGN:
    case XL6_AST_MULTI_ASSIGN:
    case XL6_AST_RETURN:
    case XL6_AST_EXPR_STMT:
    case XL6_AST_IF:
    case XL6_AST_WHILE:
    case XL6_AST_DO_WHILE:
    case XL6_AST_FOR:
    case XL6_AST_THROW:
    case XL6_AST_ASSERT:
    case XL6_AST_TRY:
    case XL6_AST_BREAK:
    case XL6_AST_CONTINUE:
    case XL6_AST_YIELD:
        return 1;
    default:
        return 0;
    }
}

static void xl6CgenDebugProbe(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const char* file;
    const char* functionName;

    if ( ctx == NULL || !ctx->debugMode || !xl6CgenDebugNodeCanProbe(node) ) {
        return;
    }

    file = ctx->debugSourcePath != NULL && ctx->debugSourcePath[0] != '\0' ? ctx->debugSourcePath : "<source>";
    functionName = "<top-level>";
    if ( ctx->currentReturnFunc != NULL &&
         ctx->currentReturnFunc->childCount > 0 &&
         ctx->currentReturnFunc->children[0] != NULL &&
         ctx->currentReturnFunc->children[0]->text != NULL ) {
        functionName = ctx->currentReturnFunc->children[0]->text;
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_debug_probe(", ctx->out);
    xl6CgenCStringLiteral(ctx->out, file);
    xl6OutputPrintf(ctx->out, ", %d, %d, ", node->line, node->column);
    xl6CgenCStringLiteral(ctx->out, xl6CgenDebugStmtKind(node));
    xl6OutputPuts(", ", ctx->out);
    xl6CgenCStringLiteral(ctx->out, functionName);
    xl6OutputPuts(");\n", ctx->out);
}
