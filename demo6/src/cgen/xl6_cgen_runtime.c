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
    xl6OutputPuts("#include <stdarg.h>\n", out);
    xl6OutputPuts("#include <string.h>\n", out);
    xl6OutputPuts("void* xrtMalloc(size_t iSize);\n\n", out);
    xl6OutputPuts("int xrtStrComp(const char* s1, const char* s2, size_t iSize, bool bCase);\n", out);
    xl6OutputPuts("const char* xrtFindStr(const char* sText, size_t iSize, const char* sSubText, size_t iSubSize, bool bCase);\n", out);
    xl6OutputPuts("bool xrtStrApprox(const char* s1, size_t len1, const char* s2, size_t len2);\n\n", out);
    xl6OutputPuts("#ifndef XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("#define XL6_XBUFFER_TYPEDEF\n", out);
    xl6OutputPuts("typedef struct xbuffer_struct { char* Buffer; unsigned int Length; unsigned int AllocLength; unsigned int AllocStep; } xbuffer_struct, *xbuffer;\n", out);
    xl6OutputPuts("#endif\n", out);
    xl6OutputPuts("void xrtBufferInit(xbuffer pBuf, unsigned int iStep);\n", out);
    xl6OutputPuts("void xrtBufferUnit(xbuffer pBuf);\n", out);
    xl6OutputPuts("bool xrtBufferAppend(xbuffer pBuf, void* pData, unsigned int iSize, unsigned int bStrMode);\n\n", out);

    xl6OutputPuts("static void xl6_xrt_fstr_append(xbuffer buffer, const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    if ( buffer == 0 || text == 0 ) {\n", out);
    xl6OutputPuts("        return;\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    xrtBufferAppend(buffer, (void*)text, 0, 1u);\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_fstr_take(xbuffer buffer)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    char* text;\n\n", out);
    xl6OutputPuts("    if ( buffer == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    if ( buffer->Buffer == 0 ) {\n", out);
    xl6OutputPuts("        xrtBufferAppend(buffer, (void*)\"\", 0, 1u);\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    text = buffer->Buffer;\n", out);
    xl6OutputPuts("    buffer->Buffer = 0;\n", out);
    xl6OutputPuts("    buffer->Length = 0;\n", out);
    xl6OutputPuts("    buffer->AllocLength = 0;\n", out);
    xl6OutputPuts("    xrtBufferUnit(buffer);\n", out);
    xl6OutputPuts("    return text != 0 ? text : \"\";\n", out);
    xl6OutputPuts("}\n\n", out);

    xl6OutputPuts("static const char* xl6_xrt_fstr_join(int count, ...)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    xbuffer_struct buffer;\n", out);
    xl6OutputPuts("    va_list args;\n", out);
    xl6OutputPuts("    int i;\n\n", out);
    xl6OutputPuts("    xrtBufferInit(&buffer, 256u);\n", out);
    xl6OutputPuts("    va_start(args, count);\n", out);
    xl6OutputPuts("    for ( i = 0; i < count; ++i ) {\n", out);
    xl6OutputPuts("        xl6_xrt_fstr_append(&buffer, va_arg(args, const char*));\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    va_end(args);\n", out);
    xl6OutputPuts("    return xl6_xrt_fstr_take(&buffer);\n", out);
    xl6OutputPuts("}\n\n", out);

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

    xl6OutputPuts("static const char* xl6_xrt_str_dup(const char* text)\n", out);
    xl6OutputPuts("{\n", out);
    xl6OutputPuts("    size_t len = text != 0 ? strlen(text) : 0;\n", out);
    xl6OutputPuts("    char* copy;\n\n", out);
    xl6OutputPuts("    if ( len == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    copy = (char*)xrtMalloc(len + 1);\n", out);
    xl6OutputPuts("    if ( copy == 0 ) {\n", out);
    xl6OutputPuts("        return \"\";\n", out);
    xl6OutputPuts("    }\n", out);
    xl6OutputPuts("    memcpy(copy, text, len);\n", out);
    xl6OutputPuts("    copy[len] = 0;\n", out);
    xl6OutputPuts("    return copy;\n", out);
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
