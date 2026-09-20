/* The result owns native stdout/stderr on every completed or failed run.
 * Public wrappers project their result before unconditionally destroying it.
 * Strings returned to take-cstr are raw xrtMalloc allocations, not xstr owners. */
void xl6CgenEmitProcessRunRuntime(XL6_Output* out, unsigned operations)
{
    if (out == NULL || operations == 0) return;
    xl6OutputPuts(
        "#include <string.h>\n"
        "static bool xl6_process_run_text(const char* command, int timeoutMs, xprocessresult* result)\n"
        "{\n"
        "    xprocessconfig config; xprocessrunoptions options;\n"
        "    memset(result, 0, sizeof(*result));\n"
        "    if (!xrtProcessShellConfigInit(&config, command != NULL ? command : \"\") ||\n"
        "        !xrtProcessRunOptionsInit(&options)) return false;\n"
        "    config.HideWindow = true; config.Stdin.Mode = XPROCESS_IO_PIPE;\n"
        "    config.Stdout.Mode = XPROCESS_IO_PIPE; config.Stderr.Mode = XPROCESS_IO_PIPE;\n"
        "    options.Deadline = timeoutMs > 0 ? xrtDeadlineAfter((uint64_t)timeoutMs * 1000u) : XRT_DEADLINE_NEVER;\n"
        "    return xrtProcessRun(&config, &options, result);\n"
        "}\n", out);
    if (operations & XL6_CGEN_PROCESS_EXEC)
        xl6OutputPuts(
            "void* xl6_xrt_process_exec(const char* command, int timeoutMs)\n"
            "{\n"
            "    xprocessresult result; char* text = NULL; size_t size;\n"
            "    (void)xl6_process_run_text(command, timeoutMs, &result);\n"
            "    size = result.Stdout != NULL ? result.StdoutSize : 0u;\n"
            "    if (size < SIZE_MAX) {\n"
            "        text = (char*)xrtMalloc(size + 1u);\n"
            "        if (text != NULL) { if (size != 0u) memcpy(text, result.Stdout, size); text[size] = '\\0'; }\n"
            "    }\n"
            "    xrtProcessResultUnit(&result);\n"
            "    return text;\n"
            "}\n", out);
    if (operations & XL6_CGEN_PROCESS_SUCCESS)
        xl6OutputPuts(
            "bool xl6_xrt_process_success(const char* command, int timeoutMs)\n"
            "{\n"
            "    xprocessresult result;\n"
            "    bool ok = xl6_process_run_text(command, timeoutMs, &result);\n"
            "    ok = ok && xrtProcessResultSuccess(&result);\n"
            "    xrtProcessResultUnit(&result);\n"
            "    return ok;\n"
            "}\n", out);
    if (operations & XL6_CGEN_PROCESS_EXIT_CODE)
        xl6OutputPuts(
            "int xl6_xrt_process_exit_code(const char* command, int timeoutMs)\n"
            "{\n"
            "    xprocessresult result; int code;\n"
            "    (void)xl6_process_run_text(command, timeoutMs, &result);\n"
            "    code = result.Status.Kind == XPROCESS_EXIT_CODE ? (int)result.Status.Code : -1;\n"
            "    xrtProcessResultUnit(&result);\n"
            "    return code;\n"
            "}\n", out);
}
