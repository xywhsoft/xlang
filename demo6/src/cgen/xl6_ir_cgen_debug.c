/* Debug instrumentation is a lowered-IR consumer.  The runtime receives only
 * canonical source spans and the function name fixed by lowering; it never
 * inspects AST nodes or reconstructs source semantics in Cgen. */
static int xl6IrCgenEmitDebugRuntime(XL6_Output* out)
{
    xl6OutputPuts(
        "#include <stdarg.h>\n"
        "typedef struct XL6_DebugState {\n"
        "    int initialized;\n"
        "    int stop_on_entry;\n"
        "    int stopped_once;\n"
        "    int step_next;\n"
        "    const char* breakpoints;\n"
        "    const char* commands;\n"
        "    size_t command_pos;\n"
        "} XL6_DebugState;\n"
        "static XL6_DebugState xl6_debug_state;\n\n"
        "static int xl6_debug_env_enabled(const char* value)\n"
        "{\n"
        "    return value != NULL && value[0] != '\\0' &&\n"
        "        strcmp(value, \"0\") != 0 && strcmp(value, \"false\") != 0;\n"
        "}\n\n"
        "static void xl6_debug_init(void)\n"
        "{\n"
        "    if (xl6_debug_state.initialized) return;\n"
        "    xl6_debug_state.initialized = 1;\n"
        "    xl6_debug_state.stop_on_entry =\n"
        "        xl6_debug_env_enabled(getenv(\"XL6_DBG_STOP_ON_ENTRY\"));\n"
        "    xl6_debug_state.breakpoints = getenv(\"XL6_DBG_BREAKPOINTS\");\n"
        "    xl6_debug_state.commands = getenv(\"XL6_DBG_COMMANDS\");\n"
        "}\n\n"
        "static int xl6_debug_line_token_matches(\n"
        "    const char* begin, const char* end, int line)\n"
        "{\n"
        "    char* parsed_end = NULL;\n"
        "    long value;\n"
        "    while (begin < end && (*begin == ' ' || *begin == '\\t')) ++begin;\n"
        "    while (end > begin && (end[-1] == ' ' || end[-1] == '\\t')) --end;\n"
        "    if (begin == end) return 0;\n"
        "    value = strtol(begin, &parsed_end, 10);\n"
        "    return parsed_end == end && value == (long)line;\n"
        "}\n\n"
        "static int xl6_debug_token_matches(\n"
        "    const char* begin, const char* end, const char* file, int line)\n"
        "{\n"
        "    const char* colon = NULL;\n"
        "    const char* cursor;\n"
        "    for (cursor = begin; cursor < end; ++cursor)\n"
        "        if (*cursor == ':') colon = cursor;\n"
        "    if (colon == NULL) return xl6_debug_line_token_matches(begin, end, line);\n"
        "    if (file == NULL || (size_t)(colon - begin) != strlen(file) ||\n"
        "        strncmp(begin, file, (size_t)(colon - begin)) != 0) return 0;\n"
        "    return xl6_debug_line_token_matches(colon + 1, end, line);\n"
        "}\n\n"
        "static int xl6_debug_has_breakpoint(const char* file, int line)\n"
        "{\n"
        "    const char* begin;\n"
        "    const char* cursor;\n"
        "    xl6_debug_init();\n"
        "    if (xl6_debug_state.breakpoints == NULL ||\n"
        "        xl6_debug_state.breakpoints[0] == '\\0') return 0;\n"
        "    begin = xl6_debug_state.breakpoints;\n"
        "    for (cursor = begin; ; ++cursor) {\n"
        "        if (*cursor == ',' || *cursor == ';' || *cursor == '\\n' ||\n"
        "            *cursor == '\\0') {\n"
        "            if (xl6_debug_token_matches(begin, cursor, file, line)) return 1;\n"
        "            if (*cursor == '\\0') break;\n"
        "            begin = cursor + 1;\n"
        "        }\n"
        "    }\n"
        "    return 0;\n"
        "}\n\n"
        "static int xl6_debug_next_command(char* command, size_t capacity)\n"
        "{\n"
        "    const char* text;\n"
        "    size_t position;\n"
        "    size_t length = 0;\n"
        "    xl6_debug_init();\n"
        "    if (command == NULL || capacity == 0) return 0;\n"
        "    command[0] = '\\0';\n"
        "    text = xl6_debug_state.commands;\n"
        "    if (text == NULL) return 0;\n"
        "    position = xl6_debug_state.command_pos;\n"
        "    while (text[position] == '\\r' || text[position] == '\\n' ||\n"
        "           text[position] == ';' || text[position] == ' ' ||\n"
        "           text[position] == '\\t') ++position;\n"
        "    if (text[position] == '\\0') {\n"
        "        xl6_debug_state.command_pos = position; return 0;\n"
        "    }\n"
        "    while (text[position] != '\\0' && text[position] != '\\r' &&\n"
        "           text[position] != '\\n' && text[position] != ';') {\n"
        "        if (length + 1 < capacity) command[length++] = text[position];\n"
        "        ++position;\n"
        "    }\n"
        "    while (length != 0 &&\n"
        "           (command[length - 1] == ' ' || command[length - 1] == '\\t'))\n"
        "        --length;\n"
        "    command[length] = '\\0';\n"
        "    xl6_debug_state.command_pos = position;\n"
        "    return length != 0;\n"
        "}\n\n"
        "static void xl6_debug_write_line(const char* format, ...)\n"
        "{\n"
        "    xstrbuf buffer;\n"
        "    va_list args;\n"
        "    int ready;\n"
        "    xrtStrBufInit(&buffer);\n"
        "    va_start(args, format);\n"
        "    ready = xrtStrBufAppendFormatV(&buffer, format, args);\n"
        "    va_end(args);\n"
        "    if (ready) (void)xrtConsoleWriteLine(\n"
        "        XCONSOLE_STDOUT, xrtStrBufView(&buffer));\n"
        "    xrtStrBufFree(&buffer);\n"
        "    (void)xrtConsoleFlush(XCONSOLE_STDOUT);\n"
        "}\n\n"
        "static void xl6_debug_probe(const char* file, int line, int column,\n"
        "                            const char* operation, const char* function_name)\n"
        "{\n"
        "    char command[64];\n"
        "    const char* reason = \"step\";\n"
        "    int should_stop = 0;\n"
        "    xl6_debug_init();\n"
        "    if (xl6_debug_state.stop_on_entry && !xl6_debug_state.stopped_once) {\n"
        "        should_stop = 1; reason = \"entry\";\n"
        "    }\n"
        "    if (xl6_debug_state.step_next) should_stop = 1;\n"
        "    if (xl6_debug_has_breakpoint(file, line)) {\n"
        "        should_stop = 1; reason = \"breakpoint\";\n"
        "    }\n"
        "    if (!should_stop) return;\n"
        "    xl6_debug_state.stopped_once = 1;\n"
        "    xl6_debug_state.step_next = 0;\n"
        "    xl6_debug_write_line(\"xl-debug stopped reason=%s file=%s line=%d column=%d "
        "function=%s kind=%s\", reason, file != NULL ? file : \"<unknown>\",\n"
        "        line, column, function_name != NULL ? function_name : \"<function>\",\n"
        "        operation != NULL ? operation : \"instruction\");\n"
        "    while (xl6_debug_next_command(command, sizeof(command))) {\n"
        "        if (strcmp(command, \"stack\") == 0 || strcmp(command, \"bt\") == 0) {\n"
        "            xl6_debug_write_line(\"xl-debug frame #0 function=%s file=%s line=%d column=%d\",\n"
        "                function_name != NULL ? function_name : \"<function>\",\n"
        "                file != NULL ? file : \"<unknown>\", line, column);\n"
        "        } else if (strcmp(command, \"scopes\") == 0) {\n"
        "            xl6_debug_write_line(\"xl-debug scope locals available=false reason=xlir-probe\");\n"
        "        } else if (strcmp(command, \"variables\") == 0 ||\n"
        "                   strcmp(command, \"vars\") == 0) {\n"
        "            xl6_debug_write_line(\"xl-debug variables function=%s kind=%s line=%d\",\n"
        "                function_name != NULL ? function_name : \"<function>\",\n"
        "                operation != NULL ? operation : \"instruction\", line);\n"
        "        } else if (strcmp(command, \"next\") == 0 ||\n"
        "                   strcmp(command, \"step\") == 0 ||\n"
        "                   strcmp(command, \"s\") == 0) {\n"
        "            xl6_debug_state.step_next = 1; break;\n"
        "        } else if (strcmp(command, \"continue\") == 0 ||\n"
        "                   strcmp(command, \"c\") == 0) {\n"
        "            break;\n"
        "        } else if (strcmp(command, \"quit\") == 0 ||\n"
        "                   strcmp(command, \"q\") == 0) {\n"
        "            exit(0);\n"
        "        } else {\n"
        "            xl6_debug_write_line(\"xl-debug unknown-command command=%s\", command);\n"
        "        }\n"
        "    }\n"
        "}\n\n", out);
    return !xl6OutputHasFailed(out);
}

static const char* xl6IrCgenDebugOperation(
    const XL6_IrInstruction* instruction)
{
    if ( instruction == NULL ) return "instruction";
    switch ( instruction->kind ) {
    case XL6_IR_INSTR_STORE: return "store";
    case XL6_IR_INSTR_CALL:
    case XL6_IR_INSTR_VIRTUAL_CALL:
    case XL6_IR_INSTR_PROTOCOL_CALL:
    case XL6_IR_INSTR_CLOSURE_CALL:
    case XL6_IR_INSTR_RUNTIME_CALL: return "call";
    case XL6_IR_INSTR_BRANCH:
    case XL6_IR_INSTR_JUMP:
    case XL6_IR_INSTR_SWITCH: return "branch";
    case XL6_IR_INSTR_RETURN: return "return";
    case XL6_IR_INSTR_THROW:
    case XL6_IR_INSTR_ERROR_RAISE: return "throw";
    case XL6_IR_INSTR_AWAIT: return "await";
    case XL6_IR_INSTR_YIELD: return "yield";
    default: return "expression";
    }
}

static void xl6IrCgenDebugCStringLiteral(XL6_Output* out, const char* text)
{
    const unsigned char* cursor = (const unsigned char*)(
        text != NULL ? text : "");
    xl6OutputPuts("\"", out);
    while ( *cursor != 0 ) {
        switch ( *cursor ) {
        case '\\': xl6OutputPuts("\\\\", out); break;
        case '"': xl6OutputPuts("\\\"", out); break;
        case '\n': xl6OutputPuts("\\n", out); break;
        case '\r': xl6OutputPuts("\\r", out); break;
        case '\t': xl6OutputPuts("\\t", out); break;
        default:
            if ( *cursor < 0x20 || *cursor == 0x7f )
                xl6OutputPrintf(out, "\\%03o", (unsigned int)*cursor);
            else
                xl6OutputPrintf(out, "%c", (int)*cursor);
            break;
        }
        ++cursor;
    }
    xl6OutputPuts("\"", out);
}

static int xl6IrCgenEmitDebugProbe(
    XL6_Output* out, const XL6_IrFunction* function,
    const XL6_IrInstruction* instruction,
    const XL6_IrCgenOptions* options)
{
    const char* source = options != NULL &&
            options->debugSourcePath != NULL &&
            options->debugSourcePath[0] != '\0'
        ? options->debugSourcePath : "<source>";
    const char* name = function != NULL && function->debugName != NULL &&
            function->debugName[0] != '\0'
        ? function->debugName : "<function>";
    if ( out == NULL || instruction == NULL ||
         instruction->sourceSpan.line <= 0 ) return 1;
    xl6OutputPuts("    xl6_debug_probe(", out);
    xl6IrCgenDebugCStringLiteral(out, source);
    xl6OutputPrintf(out, ", %d, %d, ",
        instruction->sourceSpan.line, instruction->sourceSpan.column);
    xl6IrCgenDebugCStringLiteral(
        out, xl6IrCgenDebugOperation(instruction));
    xl6OutputPuts(", ", out);
    xl6IrCgenDebugCStringLiteral(out, name);
    xl6OutputPuts(");\n", out);
    return !xl6OutputHasFailed(out);
}
