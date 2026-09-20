static void xl6CgenEmitOsRuntime(XL6_Output* out)
{
    if ( out == NULL ) {
        return;
    }

    xl6OutputPuts("#ifndef XL6_OS_RUNTIME\n", out);
    xl6OutputPuts("#define XL6_OS_RUNTIME\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#ifndef XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XVALUE_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xvalue xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("static inline char* xl6_xrt_string_dup(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("char* xrtEnvGet(const char* sName);\n\n", out);
    xl6OutputPuts("xvalue* xrtValueArray(void);\n", out);
    xl6OutputPuts("void xrtValueRelease(xvalue* pVal);\n\n", out);

    xl6OutputPuts("static xvalue* xl6_os_args_snapshot = 0;\n\n", out);

    xl6OutputPuts("void xl6_xrt_os_set_args(xvalue* args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue* copy = args != 0 ? xrtValueClone(args) : xrtValueArray();\n", out);
    xl6OutputPuts("    if ( copy == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( xl6_os_args_snapshot != 0 ) { xrtValueRelease(xl6_os_args_snapshot); }\n", out);
    xl6OutputPuts("    xl6_os_args_snapshot = copy;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_os_clear_args(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( xl6_os_args_snapshot != 0 ) {\n", out);
    xl6OutputPuts("        xrtValueRelease(xl6_os_args_snapshot);\n", out);
    xl6OutputPuts("        xl6_os_args_snapshot = 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue* xl6_xrt_os_args(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( xl6_os_args_snapshot != 0 ) {\n", out);
    xl6OutputPuts("        xvalue* copy = xrtValueClone(xl6_os_args_snapshot);\n", out);
    xl6OutputPuts("        if ( copy != 0 ) { return copy; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xrtValueArray();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_os_env(const char* name, const char* defaultValue)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* value = name != 0 ? xrtEnvGet(name) : 0;\n", out);
    xl6OutputPuts("    if ( value != 0 ) { return value; }\n", out);
    xl6OutputPuts("    return xl6_xrt_string_dup(defaultValue != 0 ? defaultValue : \"\", 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_os_platform(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("#if defined(_WIN32) || defined(_WIN64)\n", out);
    xl6OutputPuts("    return xl6_xrt_string_dup(\"windows\", 7);\n", out);
    xl6OutputPuts("#elif defined(__APPLE__)\n", out);
    xl6OutputPuts("    return xl6_xrt_string_dup(\"macos\", 5);\n", out);
    xl6OutputPuts("#elif defined(__linux__)\n", out);
    xl6OutputPuts("    return xl6_xrt_string_dup(\"linux\", 5);\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("    return xl6_xrt_string_dup(\"unknown\", 7);\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);
}
