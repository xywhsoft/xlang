static int xl6IrCgenEmitRuntimeSupport(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrCgenNeeds* needs, XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    if (needs->needsSpecialF64Runtime)
        xl6OutputPuts(
            "#include <float.h>\n#include <string.h>\n"
            "_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024, \"XLIR f64 requires IEEE binary64\");\n"
            "static double xlir_f64_from_bits(uint64_t bits)\n"
            "{ double value; _Static_assert(sizeof(value) == sizeof(bits), \"XLIR f64 width\");\n"
            "  memcpy(&value, &bits, sizeof(value)); return value; }\n", out);
    if ( needs->needsStringRuntime ) {
        xl6OutputPuts(
            "#include <string.h>\n"
            "extern char* xrtStrDup(const char*);\n"
            "extern char* xrtStrDupN(const char*, size_t);\n"
            "extern void xrtFree(void*);\n"
            "static xlir_string xlir_string_from_utf8(const char* data, size_t size)\n"
            "{ xlir_string value = { xrtStrDupN(data, size), size }; if (value.Data == NULL) value.Size = 0; return value; }\n"
            "static xlir_string xlir_string_copy(xlir_string source)\n"
            "{ return source.Data != NULL ? xlir_string_from_utf8(source.Data, source.Size) : (xlir_string){0}; }\n"
            "static xlir_string xlir_string_from_cstr(const char* source)\n"
            "{ return source != NULL ? xlir_string_from_utf8(source, strlen(source)) : (xlir_string){0}; }\n"
            "static xlir_string xlir_string_take_cstr(char* source)\n"
            "{ return source != NULL ? (xlir_string){ source, strlen(source) } : (xlir_string){0}; }\n"
            "static xlir_string xlir_string_take_known(char* source, size_t size)\n"
            "{ return source != NULL ? (xlir_string){ source, size } : (xlir_string){0}; }\n"
            "static const char* xlir_string_cstr(xlir_string source)\n"
            "{ if (source.Data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }\n"
            "  if (memchr(source.Data, 0, source.Size) != NULL) { xrtSetErrorKind(XERR_VALUE); return NULL; }\n"
            "  return source.Data; }\n"
            "static void xlir_string_drop(xlir_string value)\n"
            "{ xrtFree((void*)value.Data); }\n", out);
        xl6OutputPuts("\n", out);
    }
    if ( needs->needsStringViews ) {
        xl6OutputPuts("extern xstrview xrtStrView(const char*);\n", out);
        if ( needs->needsStringViewOperations )
            xl6OutputPuts("extern char* xrtStrConcat(xlir_string_view, xlir_string_view);\n"
                          "extern bool xrtStrEqual(xlir_string_view, xlir_string_view);\n"
                          "static xlir_string xlir_string_concat(xlir_string left, xlir_string right)\n"
                          "{\n"
                          "    char* data; size_t size;\n"
                          "    if (left.Size > SIZE_MAX - right.Size) { xrtSetErrorKind(XERR_RANGE); return (xlir_string){0}; }\n"
                          "    size = left.Size + right.Size; data = xrtStrConcat(left, right);\n"
                          "    return (xlir_string){ data, data != NULL ? size : 0 };\n"
                          "}\n", out);
        if ( needs->needsStringComparisonOperation )
            xl6OutputPuts("extern int xrtStrCompare(xlir_string_view, xlir_string_view);\n", out);
        if ( needs->needsStringSimilarityOperation )
            xl6OutputPuts("extern double xrtUtf8Similarity(xlir_string_view, xlir_string_view);\n", out);
        if ( needs->needsStringUtf8CountOperation )
            xl6OutputPuts("extern size_t xrtUtf8Count(xlir_string_view);\n", out);
        if ( needs->needsStringUtf8AtOperation )
            xl6OutputPuts("extern bool xrtUtf8Range(xlir_string_view, int64_t, int64_t, xlir_string_view*);\n"
                          "static xlir_string xlir_string_utf8_at(xlir_string text, int64_t index)\n"
                          "{ xlir_string_view part = {0}; return xrtUtf8Range(text, index, INT64_C(1), &part)\n"
                          "    ? xlir_string_from_utf8(part.Data, part.Size) : (xlir_string){0}; }\n", out);
        if ( needs->needsStringContainsOperation )
            xl6OutputPuts("extern bool xrtStrContains(xlir_string_view, xlir_string_view);\n", out);
    }
    if ( needs->needsStringScalarRuntime || needs->needsBytesRuntime )
        xl6OutputPuts(
            "#include <string.h>\n"
            "static bool xlir_sequence_index(size_t length, int64_t index, size_t* resolved)\n"
            "{\n"
            "    uint64_t magnitude;\n"
            "    if (resolved == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "    if (index >= 0) {\n"
            "        if ((uint64_t)index >= (uint64_t)length) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "        *resolved = (size_t)index; return true;\n"
            "    }\n"
            "    magnitude = (uint64_t)(-(index + INT64_C(1))) + UINT64_C(1);\n"
            "    if (magnitude > (uint64_t)length) { xrtSetErrorKind(XERR_RANGE); return false; }\n"
            "    *resolved = length - (size_t)magnitude; return true;\n"
            "}\n"
            , out);
    if ( needs->needsStringScalarRuntime )
        xl6OutputPuts(
            "static xlir_string xlir_char_to_string(uint32_t scalar)\n"
            "{\n"
            "    char encoded[4]; size_t size = xrtUtf8Encode((uint32)scalar, encoded);\n"
            "    if (size == 0) { xrtSetErrorKind(XERR_VALUE); return (xlir_string){0}; }\n"
            "    return xlir_string_from_utf8(encoded, size);\n"
            "}\n"
            "static bool xlir_string_scalar_at(xlir_string text, int64_t index, uint32_t* scalar)\n"
            "{\n"
            "    xstrview view; size_t count; size_t resolved; uint32 value;\n"
            "    if (text.Data == NULL || scalar == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "    view = text; count = xrtUtf8Count(view);\n"
            "    if (count == XRT_NPOS) { xrtSetErrorKind(XERR_VALUE); return false; }\n"
            "    if (!xlir_sequence_index(count, index, &resolved)) return false;\n"
            "    if (!xrtUtf8At(view, resolved, &value)) { xrtSetErrorKind(XERR_VALUE); return false; }\n"
            "    *scalar = (uint32_t)value; return true;\n"
            "}\n"
            "static xlir_string xlir_string_scalar_set_copy(xlir_string text, int64_t index, uint32_t scalar)\n"
            "{\n"
            "    xstrview view; size_t count; size_t resolved; size_t start; size_t end;\n"
            "    size_t encodedSize; size_t removed; size_t newSize; char encoded[4]; char* result;\n"
            "    if (text.Data == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return (xlir_string){0}; }\n"
            "    view = text; count = xrtUtf8Count(view);\n"
            "    if (count == XRT_NPOS) { xrtSetErrorKind(XERR_VALUE); return (xlir_string){0}; }\n"
            "    if (!xlir_sequence_index(count, index, &resolved)) return (xlir_string){0};\n"
            "    start = xrtUtf8Offset(view, resolved); end = xrtUtf8Offset(view, resolved + 1u);\n"
            "    encodedSize = xrtUtf8Encode((uint32)scalar, encoded);\n"
            "    if (start == XRT_NPOS || end == XRT_NPOS || end < start || encodedSize == 0) {\n"
            "        xrtSetErrorKind(XERR_VALUE); return (xlir_string){0};\n"
            "    }\n"
            "    removed = end - start;\n"
            "    if (view.Size - removed > SIZE_MAX - encodedSize ||\n"
            "        view.Size - removed + encodedSize == SIZE_MAX) {\n"
            "        xrtSetErrorKind(XERR_RANGE); return (xlir_string){0};\n"
            "    }\n"
            "    newSize = view.Size - removed + encodedSize;\n"
            "    result = (char*)xrtMalloc(newSize + 1u);\n"
            "    if (result == NULL) return (xlir_string){0};\n"
            "    if (start != 0) memcpy(result, view.Data, start);\n"
            "    memcpy(result + start, encoded, encodedSize);\n"
            "    if (end != view.Size) memcpy(result + start + encodedSize, view.Data + end, view.Size - end);\n"
            "    result[newSize] = '\\0'; return (xlir_string){result, newSize};\n"
            "}\n"
            "static xlir_string xlir_string_concat_char_right(xlir_string text, uint32_t scalar)\n"
            "{\n"
            "    char encoded[4]; size_t size = xrtUtf8Encode((uint32)scalar, encoded);\n"
            "    if (text.Data == NULL || size == 0 || text.Size > SIZE_MAX - size) { xrtSetErrorKind(XERR_VALUE); return (xlir_string){0}; }\n"
            "    return xlir_string_take_known(xrtStrConcat(text, (xstrview){ encoded, size }), text.Size + size);\n"
            "}\n"
            "static xlir_string xlir_string_concat_char_left(uint32_t scalar, xlir_string text)\n"
            "{\n"
            "    char encoded[4]; size_t size = xrtUtf8Encode((uint32)scalar, encoded);\n"
            "    if (text.Data == NULL || size == 0 || text.Size > SIZE_MAX - size) { xrtSetErrorKind(XERR_VALUE); return (xlir_string){0}; }\n"
            "    return xlir_string_take_known(xrtStrConcat((xstrview){ encoded, size }, text), text.Size + size);\n"
            "}\n", out);
    if ( needs->needsBytesRuntime )
        xl6OutputPuts(
            "static xbytesview xlir_bytes_view_exact(const xvalue* value)\n"
            "{\n"
            "    xbytesview view = { NULL, 0 };\n"
            "    if (!xrtValueGetBytes(value, &view)) abort();\n"
            "    return view;\n"
            "}\n"
            "static int xlir_bytes_compare(const xvalue* left, const xvalue* right)\n"
            "{\n"
            "    xbytesview a = xlir_bytes_view_exact(left);\n"
            "    xbytesview b = xlir_bytes_view_exact(right);\n"
            "    size_t common = a.Size < b.Size ? a.Size : b.Size;\n"
            "    int order = common != 0 ? memcmp(a.Data, b.Data, common) : 0;\n"
            "    if (order != 0) return order;\n"
            "    return a.Size < b.Size ? -1 : a.Size > b.Size ? 1 : 0;\n"
            "}\n"
            "static int64_t xlir_bytes_length(const xvalue* value)\n"
            "{\n"
            "    xbytesview view = xlir_bytes_view_exact(value);\n"
            "    if (view.Size > (size_t)INT64_MAX) abort();\n"
            "    return (int64_t)view.Size;\n"
            "}\n"
            "static xvalue* xlir_bytes_concat(const xvalue* left, const xvalue* right)\n"
            "{\n"
            "    xbytesview a = xlir_bytes_view_exact(left);\n"
            "    xbytesview b = xlir_bytes_view_exact(right); bytes data; xvalue* result; size_t size;\n"
            "    if (a.Size > SIZE_MAX - b.Size) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
            "    size = a.Size + b.Size; data = (bytes)xrtMalloc(size != 0 ? size : 1u);\n"
            "    if (data == NULL) return NULL;\n"
            "    if (a.Size != 0) memcpy(data, a.Data, a.Size);\n"
            "    if (b.Size != 0) memcpy(data + a.Size, b.Data, b.Size);\n"
            "    result = xrtValueBytesTake(&data, size); xrtFree(data); return result;\n"
            "}\n"
            "static bool xlir_bytes_at(const xvalue* value, int64_t index, int64_t* item)\n"
            "{\n"
            "    xbytesview view = xlir_bytes_view_exact(value); size_t resolved;\n"
            "    if (item == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }\n"
            "    if (!xlir_sequence_index(view.Size, index, &resolved)) return false;\n"
            "    *item = (int64_t)view.Data[resolved]; return true;\n"
            "}\n"
            "static xvalue* xlir_bytes_set_copy(const xvalue* value, int64_t index, int64_t item)\n"
            "{\n"
            "    xbytesview view = xlir_bytes_view_exact(value); size_t resolved; bytes data; xvalue* result;\n"
            "    if ((uint64_t)item > UINT64_C(255)) { xrtSetErrorKind(XERR_RANGE); return NULL; }\n"
            "    if (!xlir_sequence_index(view.Size, index, &resolved)) return NULL;\n"
            "    data = (bytes)xrtMalloc(view.Size != 0 ? view.Size : 1u);\n"
            "    if (data == NULL) return NULL;\n"
            "    if (view.Size != 0) memcpy(data, view.Data, view.Size);\n"
            "    data[resolved] = (unsigned char)item;\n"
            "    result = xrtValueBytesTake(&data, view.Size); xrtFree(data); return result;\n"
            "}\n", out);
    if ( needs->needsScalarApproxRuntime )
        xl6OutputPuts(
            "static bool xlir_i64_approx(int64_t left, int64_t right)\n"
            "{\n"
            "    long double a; long double b; long double difference; long double maximum;\n"
            "    if (left == right) return true;\n"
            "    a = (long double)left; b = (long double)right;\n"
            "    difference = a > b ? a - b : b - a;\n"
            "    a = a < 0.0L ? -a : a; b = b < 0.0L ? -b : b;\n"
            "    maximum = a > b ? a : b;\n"
            "    return maximum != 0.0L && difference / maximum <= 0.0001L;\n"
            "}\n"
            "static bool xlir_u64_approx(uint64_t left, uint64_t right)\n"
            "{\n"
            "    uint64_t difference; uint64_t maximum;\n"
            "    if (left == right) return true;\n"
            "    difference = left > right ? left - right : right - left;\n"
            "    maximum = left > right ? left : right;\n"
            "    return maximum != 0 && (long double)difference / (long double)maximum <= 0.0001L;\n"
            "}\n"
            "static bool xlir_f64_approx(double left, double right)\n"
            "{\n"
            "    double difference;\n"
            "    if (left == right) return true;\n"
            "    difference = left > right ? left - right : right - left;\n"
            "    return difference <= 0.01;\n"
             "}\n", out);
    if ( needs->needsI64PowRuntime )
        xl6OutputPuts(
            "static int64_t xlir_i64_pow(int64_t base, int64_t exponent)\n"
            "{\n"
            "    int64_t result = 1;\n"
            "    if (exponent < 0) return 0;\n"
            "    while (exponent > 0) {\n"
            "        if ((exponent & 1) != 0) result *= base;\n"
            "        exponent >>= 1;\n"
            "        if (exponent > 0) base *= base;\n"
            "    }\n"
            "    return result;\n"
            "}\n", out);
    if ( needs->needsTimeApproxRuntime )
        xl6OutputPuts(
            "extern bool xrtTimeNear(int64_t, int64_t, uint64_t);\n", out);
    if ( needs->needsI64RangeContainsRuntime )
        xl6OutputPuts(
            "static bool xlir_i64_range_contains(int64_t item, int64_t start, int64_t stop, int64_t step)\n"
            "{\n"
            "    uint64_t distance; uint64_t magnitude;\n"
            "    if (step == 0) return false;\n"
            "    if (step > 0) {\n"
            "        if (item < start || item >= stop) return false;\n"
            "        distance = (uint64_t)item - (uint64_t)start;\n"
            "        magnitude = (uint64_t)step;\n"
            "    } else {\n"
            "        if (item > start || item <= stop) return false;\n"
            "        distance = (uint64_t)start - (uint64_t)item;\n"
            "        magnitude = (uint64_t)0 - (uint64_t)step;\n"
            "    }\n"
            "    return distance % magnitude == 0;\n"
            "}\n", out);
    return !xl6OutputHasFailed(out);
}
