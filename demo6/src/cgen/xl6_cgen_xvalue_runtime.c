static void xl6CgenEmitXValueReleaseConvertHelpers(XL6_Output* out)
{
    xl6OutputPuts("#ifndef XL6_XRT_VALUE_RELEASE_CONVERT_HELPERS\n", out);
    xl6OutputPuts("#define XL6_XRT_VALUE_RELEASE_CONVERT_HELPERS\n", out);
    xl6OutputPuts("static long long xl6_xrt_value_to_i64_release(xvalue* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    long long result = value != 0 ? xl6_xrt_value_int(value) : 0;\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xrtValueRelease);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static double xl6_xrt_value_to_f64_release(xvalue* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    double result = value != 0 ? xl6_xrt_value_float(value) : 0.0;\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xrtValueRelease);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static int xl6_xrt_value_to_bool_release(xvalue* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    int result = value != 0 && xl6_xrt_value_bool(value) ? 1 : 0;\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xrtValueRelease);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static long long xl6_xrt_value_to_time_release(xvalue* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    long long result = value != 0 ? xl6_xrt_value_time(value) : 0;\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xrtValueRelease);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static void* xl6_xrt_value_to_point_release(xvalue* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    void* result = value != 0 ? xl6_xrt_value_pointer(value) : 0;\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xrtValueRelease);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);
}



static void xl6CgenEmitTypeConvertRuntime(XL6_CgenContext* ctx)
{
    XL6_Output* out = ctx->out;
    const XL6_AstNode* root = ctx->root;
    int needsHexBuffer = xl6CgenNodeHasExternSymbol(root, "xl6_xrt_buffer_from_hex");
    int needsBase64Buffer = xl6CgenNodeHasExternSymbol(root, "xl6_xrt_buffer_from_base64");
    int needsHex = xl6CgenNodeHasExternSymbol(root, "xl6_xrt_hex_encode_text") ||
        xl6CgenNodeHasExternSymbol(root, "xl6_xrt_hex_encode_bytes") ||
        xl6CgenNodeHasExternSymbol(root, "xl6_xrt_hex_decode_text") ||
        needsHexBuffer;
    int needsBase64 = xl6CgenNodeHasExternSymbol(root, "xl6_xrt_base64_encode_text") ||
        xl6CgenNodeHasExternSymbol(root, "xl6_xrt_base64_encode_bytes") ||
        xl6CgenNodeHasExternSymbol(root, "xl6_xrt_base64_decode_text") ||
        needsBase64Buffer;
    int needsPercent = xl6CgenNodeHasExternSymbol(root, "xl6_xrt_percent_encode_text") ||
        xl6CgenNodeHasExternSymbol(root, "xl6_xrt_percent_decode_text");

    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <errno.h>\n", out);
    xl6OutputPuts("#include <stdlib.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);

    /* XRT 类型与函数只由 xl_runtime.h 中的声明 SDK 提供。 */
    xl6OutputPuts("\n", out);
    xl6OutputPuts("static void xl6_error_raise(const char* kind, const char* message, const char* file, int line, int column);\n\n", out);

    xl6OutputPuts("#ifndef XL6_STRICT_STRING_CONVERT_HELPERS\n", out);
    xl6OutputPuts("#define XL6_STRICT_STRING_CONVERT_HELPERS\n", out);
    xl6OutputPuts("static int xl6_convert_is_space(char ch)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return ch == ' ' || ch == '\\t' || ch == '\\r' || ch == '\\n' || ch == '\\v' || ch == '\\f';\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static long long xl6_xrt_str_to_i64_checked(const char* text, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* end = 0;\n", out);
    xl6OutputPuts("    long long value;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    while ( xl6_convert_is_space(*text) ) { ++text; }\n", out);
    xl6OutputPuts("    errno = 0;\n", out);
    xl6OutputPuts("    value = strtoll(text, &end, 10);\n", out);
    xl6OutputPuts("    if ( end == text || errno == ERANGE ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ValueError\", \"invalid integer string\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    while ( end != 0 && xl6_convert_is_space(*end) ) { ++end; }\n", out);
    xl6OutputPuts("    if ( end == 0 || *end != '\\0' ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ValueError\", \"invalid integer string\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static unsigned long long xl6_xrt_str_to_u64_checked(const char* text, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* end = 0;\n", out);
    xl6OutputPuts("    unsigned long long value;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    while ( xl6_convert_is_space(*text) ) { ++text; }\n", out);
    xl6OutputPuts("    if ( *text == '-' ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"RangeError\", \"negative string cannot convert to unsigned integer\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    errno = 0;\n", out);
    xl6OutputPuts("    value = strtoull(text, &end, 10);\n", out);
    xl6OutputPuts("    if ( end == text || errno == ERANGE ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ValueError\", \"invalid unsigned integer string\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    while ( end != 0 && xl6_convert_is_space(*end) ) { ++end; }\n", out);
    xl6OutputPuts("    if ( end == 0 || *end != '\\0' ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ValueError\", \"invalid unsigned integer string\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static double xl6_xrt_str_to_f64_checked(const char* text, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* end = 0;\n", out);
    xl6OutputPuts("    double value;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    while ( xl6_convert_is_space(*text) ) { ++text; }\n", out);
    xl6OutputPuts("    errno = 0;\n", out);
    xl6OutputPuts("    value = strtod(text, &end);\n", out);
    xl6OutputPuts("    if ( end == text || errno == ERANGE ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ValueError\", \"invalid float string\", file, line, column);\n", out);
    xl6OutputPuts("        return 0.0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    while ( end != 0 && xl6_convert_is_space(*end) ) { ++end; }\n", out);
    xl6OutputPuts("    if ( end == 0 || *end != '\\0' ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"ValueError\", \"invalid float string\", file, line, column);\n", out);
    xl6OutputPuts("        return 0.0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("\nstatic long long xl6_xrt_f64_to_i64_checked(double value, int bits, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    double limit = bits >= 64 ? 9223372036854775808.0 : (double)(1ULL << (bits - 1));\n", out);
    xl6OutputPuts("    if ( value != value || value < -limit || value >= limit ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"RangeError\", \"float value is outside integer range\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return (long long)value;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static unsigned long long xl6_xrt_f64_to_u64_checked(double value, int bits, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    double limit = bits >= 64 ? 18446744073709551616.0 : (double)(1ULL << bits);\n", out);
    xl6OutputPuts("    if ( value != value || value < 0.0 || value >= limit ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"RangeError\", \"float value is outside unsigned integer range\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return (unsigned long long)value;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static long long xl6_xrt_i64_range_checked(long long value, int bits, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( bits < 64 ) {\n", out);
    xl6OutputPuts("        long long limit = (long long)(1ULL << (bits - 1));\n", out);
    xl6OutputPuts("        if ( value < -limit || value >= limit ) {\n", out);
    xl6OutputPuts("            xl6_error_raise(\"RangeError\", \"integer value is outside target range\", file, line, column);\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static unsigned long long xl6_xrt_u64_range_checked(unsigned long long value, int bits, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( bits < 64 && value >= (1ULL << bits) ) {\n", out);
    xl6OutputPuts("        xl6_error_raise(\"RangeError\", \"unsigned integer value is outside target range\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return value;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static long long xl6_xrt_value_to_i64_checked(const xvalue* value, int bits, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( value == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    switch ( xrtValueType(value) ) {\n", out);
    xl6OutputPuts("    case XVALUE_NULL: return 0;\n", out);
    xl6OutputPuts("    case XVALUE_BOOL: return xl6_xrt_value_bool(value) ? 1 : 0;\n", out);
    xl6OutputPuts("    case XVALUE_INT: return xl6_xrt_i64_range_checked(xl6_xrt_value_int(value), bits, file, line, column);\n", out);
    xl6OutputPuts("    case XVALUE_FLOAT: return xl6_xrt_f64_to_i64_checked(xl6_xrt_value_float(value), bits, file, line, column);\n", out);
    xl6OutputPuts("    case XVALUE_STRING: return xl6_xrt_i64_range_checked(xl6_xrt_str_to_i64_checked(xl6_xrt_value_text(value), file, line, column), bits, file, line, column);\n", out);
    xl6OutputPuts("    default:\n", out);
    xl6OutputPuts("        xl6_error_raise(\"TypeError\", \"dynamic value cannot convert to integer\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static unsigned long long xl6_xrt_value_to_u64_checked(const xvalue* value, int bits, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    long long integer;\n", out);
    xl6OutputPuts("    if ( value == 0 ) { return 0; }\n", out);
    xl6OutputPuts("    switch ( xrtValueType(value) ) {\n", out);
    xl6OutputPuts("    case XVALUE_NULL: return 0;\n", out);
    xl6OutputPuts("    case XVALUE_BOOL: return xl6_xrt_value_bool(value) ? 1u : 0u;\n", out);
    xl6OutputPuts("    case XVALUE_INT:\n", out);
    xl6OutputPuts("        integer = xl6_xrt_value_int(value);\n", out);
    xl6OutputPuts("        if ( integer < 0 ) { xl6_error_raise(\"RangeError\", \"negative value cannot convert to unsigned integer\", file, line, column); return 0; }\n", out);
    xl6OutputPuts("        return xl6_xrt_u64_range_checked((unsigned long long)integer, bits, file, line, column);\n", out);
    xl6OutputPuts("    case XVALUE_FLOAT: return xl6_xrt_f64_to_u64_checked(xl6_xrt_value_float(value), bits, file, line, column);\n", out);
    xl6OutputPuts("    case XVALUE_STRING: return xl6_xrt_u64_range_checked(xl6_xrt_str_to_u64_checked(xl6_xrt_value_text(value), file, line, column), bits, file, line, column);\n", out);
    xl6OutputPuts("    default:\n", out);
    xl6OutputPuts("        xl6_error_raise(\"TypeError\", \"dynamic value cannot convert to unsigned integer\", file, line, column);\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static long long xl6_xrt_value_to_i64_checked_release(xvalue* value, int bits, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    long long result = xl6_xrt_value_to_i64_checked(value, bits, file, line, column);\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xrtValueRelease); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static unsigned long long xl6_xrt_value_to_u64_checked_release(xvalue* value, int bits, const char* file, int line, int column)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    unsigned long long result = xl6_xrt_value_to_u64_checked(value, bits, file, line, column);\n", out);
    xl6OutputPuts("    if ( value != 0 ) { xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xrtValueRelease); }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n", out);
    xl6OutputPuts("#endif\n\n", out);

    xl6OutputPuts("#ifndef XL6_XRT_TAKE_CSTR_HELPERS\n", out);
    xl6OutputPuts("#define XL6_XRT_TAKE_CSTR_HELPERS\n", out);
    xl6OutputPuts("static const char* xl6_xrt_temp_cstr(const char* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    if ( value == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    len = strlen(value);\n", out);
    xl6OutputPuts("    text = (char*)xrtTemp(len + 1);\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(text, value, len + 1);\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("const char* xl6_xrt_take_cstr(void* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* text = xl6_xrt_temp_cstr((const char*)value);\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        xrtFree(value);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);

    if ( needsHex ) {
    xl6OutputPuts("void* xl6_xrt_hex_encode_text(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    if ( size == 0u ) { size = strlen(text); }\n", out);
    xl6OutputPuts("    return xrtHexEncodeNew(text, size, 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6CgenEmitHexBytesCodecRuntime(out, 1);

    xl6OutputPuts("/* 严格解码 HEX 文本并返回带末尾零的所有权结果。 */\n", out);
    xl6OutputPuts("void* xl6_xrt_hex_decode_text(const char* text, size_t size)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xstrview view;\n", out);
    xl6OutputPuts("    size_t outputSize = 0u;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    if ( size == 0u ) { size = strlen(text); }\n", out);
    xl6OutputPuts("    view.Data = text;\n", out);
    xl6OutputPuts("    view.Size = size;\n", out);
    xl6OutputPuts("    return xrtHexDecodeNew(view, &outputSize, 0u);\n", out);
    xl6OutputPuts("}\n\n", out);
    }

    if ( needsBase64 ) {
        xl6CgenEmitBase64TextCodecRuntime(out, 1);
        xl6CgenEmitBase64BytesCodecRuntime(out, 1);
    }

    if ( needsPercent ) {
    xl6OutputPuts("/* 使用 XRT 核心的 RFC 3986 百分号编码，不混入表单编码策略。 */\n", out);
    xl6OutputPuts("void* xl6_xrt_percent_encode_text(const char* text, size_t size, void* outSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t ignoredSize = 0u;\n", out);
    xl6OutputPuts("    size_t* outputSize = outSize != 0 ? (size_t*)outSize : &ignoredSize;\n", out);
    xl6OutputPuts("    xstrview extraSafe = { 0, 0u };\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    return xrtPercentEncodeNew(text, size, extraSafe, outputSize);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("/* 使用 XRT 核心的严格百分号解码，保留加号原义。 */\n", out);
    xl6OutputPuts("void* xl6_xrt_percent_decode_text(const char* text, size_t size, void* outSize)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t ignoredSize = 0u;\n", out);
    xl6OutputPuts("    size_t* outputSize = outSize != 0 ? (size_t*)outSize : &ignoredSize;\n", out);
    xl6OutputPuts("    xstrview view;\n", out);
    xl6OutputPuts("    if ( text == 0 ) { text = \"\"; }\n", out);
    xl6OutputPuts("    view.Data = text;\n", out);
    xl6OutputPuts("    view.Size = size;\n", out);
    xl6OutputPuts("    return xrtPercentDecodeNew(view, outputSize);\n", out);
    xl6OutputPuts("}\n\n", out);
    }

    xl6OutputPuts("static const char* xl6_xrt_i64_to_string(long long value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char buffer[64];\n", out);
    xl6OutputPuts("    size_t size = 0;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    if ( !xrtIntWrite(value, 10u, buffer, sizeof(buffer), &size, 0u) ) { return \"\"; }\n", out);
    xl6OutputPuts("    text = (char*)xrtTemp(size + 1u);\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(text, buffer, size + 1u);\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_u64_to_string(unsigned long long value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char buffer[64];\n", out);
    xl6OutputPuts("    size_t size = 0;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    if ( !xrtUIntWrite(value, 10u, buffer, sizeof(buffer), &size, 0u) ) { return \"\"; }\n", out);
    xl6OutputPuts("    text = (char*)xrtTemp(size + 1u);\n", out);
    xl6OutputPuts("    if ( text == 0 ) { return \"\"; }\n", out);
    xl6OutputPuts("    memcpy(text, buffer, size + 1u);\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_f64_to_string(double value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char buffer[128];\n", out);
    xl6OutputPuts("    size_t size = 0;\n", out);
    xl6OutputPuts("    char* text;\n", out);
    xl6OutputPuts("    if ( !xrtNumWrite(value, buffer, sizeof(buffer), &size, 0u) ) { return \"\"; }\n", out);
    xl6OutputPuts("    text = (char*)xrtTemp(size + 1u);\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(text, buffer, size + 1u);\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#ifndef XL6_XRT_VALUE_STRING_HELPERS\n", out);
    xl6OutputPuts("#define XL6_XRT_VALUE_STRING_HELPERS\n", out);
    xl6OutputPuts("static const char* xl6_xrt_value_to_string(xvalue* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    return xl6_xrt_take_cstr((char*)xl6_xrt_value_string_owned(value, 0));\n", out);
    xl6OutputPuts("}\n\n", out);
    xl6OutputPuts("static const char* xl6_xrt_value_to_string_release(xvalue* value)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    const char* text = xl6_xrt_value_to_string(value);\n", out);
    xl6OutputPuts("    if ( value != 0 ) {\n", out);
    xl6OutputPuts("        xl6_temp_managed_release((void*)value, (XL6_TempManagedDrop)xrtValueRelease);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("#endif\n\n", out);

    xl6CgenEmitXValueReleaseConvertHelpers(out);
}
