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
        xl6OutputPuts("extern char* xrtStrDup(const char*);\nextern void xrtFree(void*);\n", out);
        xl6OutputPuts("\n", out);
    }
    if ( needs->needsStringViews ) {
        xl6OutputPuts("extern xstrview xrtStrView(const char*);\n", out);
        if ( needs->needsStringViewOperations )
            xl6OutputPuts("extern char* xrtStrConcat(xlir_string_view, xlir_string_view);\n"
                          "extern bool xrtStrEqual(xlir_string_view, xlir_string_view);\n", out);
        if ( needs->needsStringComparisonOperation )
            xl6OutputPuts("extern int xrtStrCompare(xlir_string_view, xlir_string_view);\n", out);
        if ( needs->needsStringSimilarityOperation )
            xl6OutputPuts("extern double xrtUtf8Similarity(xlir_string_view, xlir_string_view);\n", out);
        if ( needs->needsStringUtf8CountOperation )
            xl6OutputPuts("extern size_t xrtUtf8Count(xlir_string_view);\n", out);
        if ( needs->needsStringUtf8AtOperation )
            xl6OutputPuts("extern char* xrtUtf8Substr(xlir_string_view, int64_t, int64_t);\n", out);
        if ( needs->needsStringContainsOperation )
            xl6OutputPuts("extern bool xrtStrContains(xlir_string_view, xlir_string_view);\n", out);
    }
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
