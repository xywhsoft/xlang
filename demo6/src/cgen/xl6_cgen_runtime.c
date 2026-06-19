#include "src/cgen/xl6_cgen_runtime.h"

void xl6CgenEmitApproxProtos(XL6_Output* out)
{
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("bool xrtIntApprox(long long a, long long b);\n", out);
    xl6OutputPuts("bool xrtNumApprox(double a, double b);\n\n", out);
    xl6OutputPuts("bool xrtTimeApprox(long long a, long long b);\n\n", out);
}

void xl6CgenEmitTimeRuntime(XL6_Output* out)
{
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("long long xrtTimeSerial(int iHour, int iMinute, int iSecond);\n", out);
    xl6OutputPuts("long long xrtDateSerial(long long iYear, int iMonth, int iDay);\n", out);
    xl6OutputPuts("long long xrtDateTimeSerial(long long iYear, int iMonth, int iDay, int iHour, int iMinute, int iSecond);\n\n", out);
    xl6OutputPuts("long long xrtStrToTime(const char* sTime, size_t iSize);\n\n", out);
}

void xl6CgenEmitStringRuntime(XL6_Output* out)
{
    xl6OutputPuts("#include <stdbool.h>\n", out);
    xl6OutputPuts("#include <stddef.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n\n", out);
    xl6OutputPuts("int xrtStrComp(const char* s1, const char* s2, size_t iSize, bool bCase);\n", out);
    xl6OutputPuts("const char* xrtFindStr(const char* sText, size_t iSize, const char* sSubText, size_t iSubSize, bool bCase);\n", out);
    xl6OutputPuts("bool xrtStrApprox(const char* s1, size_t len1, const char* s2, size_t len2);\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_str_concat(const char* left, const char* right)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t leftLen = left != 0 ? strlen(left) : 0;\n", out);
    xl6OutputPuts("    size_t rightLen = right != 0 ? strlen(right) : 0;\n", out);
    xl6OutputPuts("    size_t totalLen = leftLen + rightLen;\n", out);
    xl6OutputPuts("    char* text;\n\n", out);
    xl6OutputPuts("    if ( totalLen == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text = (char*)xrtMalloc(totalLen + 1);\n", out);
    xl6OutputPuts("    if ( text == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( leftLen > 0 ) {\n", out);
    xl6OutputPuts("        memcpy(text, left, leftLen);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( rightLen > 0 ) {\n", out);
    xl6OutputPuts("        memcpy(text + leftLen, right, rightLen);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text[totalLen] = 0;\n", out);
    xl6OutputPuts("    return text;\n", out);
    xl6OutputPuts("}\n\n", out);
}

void xl6CgenEmitIntPowHelper(XL6_Output* out)
{
    xl6OutputPuts("static long long xl6_int_pow_i64(long long base, long long exp)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    long long result = 1;\n\n", out);
    xl6OutputPuts("    if ( exp < 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    while ( exp > 0 ) {\n", out);
    xl6OutputPuts("        if ( (exp & 1) != 0 ) {\n", out);
    xl6OutputPuts("            result *= base;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        exp >>= 1;\n", out);
    xl6OutputPuts("        if ( exp > 0 ) {\n", out);
    xl6OutputPuts("            base *= base;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return result;\n", out);
    xl6OutputPuts("}\n\n", out);
}

void xl6CgenEmitRangeContainsHelper(XL6_Output* out)
{
    xl6OutputPuts("static int xl6_range_contains_i64(long long value, long long start, long long stop, long long step)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( step == 0 ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( step > 0 ) {\n", out);
    xl6OutputPuts("        if ( value < start || value >= stop ) {\n", out);
    xl6OutputPuts("            return 0;\n", out);
    xl6OutputPuts("        }\n", out);
    xl6OutputPuts("        return ((value - start) % step) == 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( value > start || value <= stop ) {\n", out);
    xl6OutputPuts("        return 0;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    return ((start - value) % -step) == 0;\n", out);
    xl6OutputPuts("}\n\n", out);
}
