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
    xl6OutputPuts("typedef struct xvalue_struct* xvalue;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("char* xrtCopyStr(const char* sText, size_t iSize);\n", out);
    xl6OutputPuts("char* xrtEnvGet(const char* sName);\n\n", out);
    xl6OutputPuts("xvalue xvoCreateArray(void);\n", out);
    xl6OutputPuts("xvalue xvoCopy(xvalue pVal);\n", out);
    xl6OutputPuts("void xvoUnref(xvalue pVal);\n\n", out);

    xl6OutputPuts("static xvalue xl6_os_args_snapshot = 0;\n\n", out);

    xl6OutputPuts("void xl6_xrt_os_set_args(xvalue args)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xvalue copy = args != 0 ? xvoCopy(args) : xvoCreateArray();\n", out);
    xl6OutputPuts("    if ( copy == 0 ) { return; }\n", out);
    xl6OutputPuts("    if ( xl6_os_args_snapshot != 0 ) { xvoUnref(xl6_os_args_snapshot); }\n", out);
    xl6OutputPuts("    xl6_os_args_snapshot = copy;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void xl6_xrt_os_clear_args(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( xl6_os_args_snapshot != 0 ) {\n", out);
    xl6OutputPuts("        xvoUnref(xl6_os_args_snapshot);\n", out);
    xl6OutputPuts("        xl6_os_args_snapshot = 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("xvalue xl6_xrt_os_args(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( xl6_os_args_snapshot != 0 ) {\n", out);
    xl6OutputPuts("        xvalue copy = xvoCopy(xl6_os_args_snapshot);\n", out);
    xl6OutputPuts("        if ( copy != 0 ) { return copy; }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return xvoCreateArray();\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_os_env(const char* name, const char* defaultValue)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* value = name != 0 ? xrtEnvGet(name) : 0;\n", out);
    xl6OutputPuts("    if ( value != 0 ) { return value; }\n", out);
    xl6OutputPuts("    return xrtCopyStr(defaultValue != 0 ? defaultValue : \"\", 0);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("void* xl6_xrt_os_platform(void)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("#if defined(_WIN32) || defined(_WIN64)\n", out);
    xl6OutputPuts("    return xrtCopyStr(\"windows\", 7);\n", out);
    xl6OutputPuts("#elif defined(__APPLE__)\n", out);
    xl6OutputPuts("    return xrtCopyStr(\"macos\", 5);\n", out);
    xl6OutputPuts("#elif defined(__linux__)\n", out);
    xl6OutputPuts("    return xrtCopyStr(\"linux\", 5);\n", out);
    xl6OutputPuts("#else\n", out);
    xl6OutputPuts("    return xrtCopyStr(\"unknown\", 7);\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);
}
