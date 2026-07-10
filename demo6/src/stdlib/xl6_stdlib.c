#include "src/stdlib/xl6_stdlib.h"
#include "src/stdlib/xl6_stdlib_data.h"

#include "src/common/xl6_common.h"
#include "src/parser/xl6_parser_api.h"

#include <stdio.h>

typedef struct XL6_StdlibTextBuilder {
    char* data;
    size_t len;
    size_t capacity;
    int ok;
} XL6_StdlibTextBuilder;

static int xl6StdlibTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static char* xl6StdlibDupText(const char* text)
{
    const char* src = text != NULL ? text : "";
    return xrtCopyStr((char*)src, strlen(src));
}

static void xl6StdlibBuilderInit(XL6_StdlibTextBuilder* builder)
{
    memset(builder, 0, sizeof(*builder));
    builder->ok = 1;
}

static void xl6StdlibBuilderFree(XL6_StdlibTextBuilder* builder)
{
    if ( builder == NULL ) {
        return;
    }
    xrtFree(builder->data);
    memset(builder, 0, sizeof(*builder));
}

static int xl6StdlibBuilderGrow(XL6_StdlibTextBuilder* builder, size_t addLen)
{
    char* newData;
    size_t required;
    size_t newCapacity;

    if ( builder == NULL || !builder->ok ) {
        return 0;
    }
    required = builder->len + addLen + 1;
    if ( required <= builder->capacity ) {
        return 1;
    }
    newCapacity = builder->capacity == 0 ? 512 : builder->capacity;
    while ( newCapacity < required ) {
        newCapacity *= 2;
    }
    newData = (char*)xrtRealloc(builder->data, newCapacity);
    if ( newData == NULL ) {
        builder->ok = 0;
        return 0;
    }
    builder->data = newData;
    builder->capacity = newCapacity;
    return 1;
}

static void xl6StdlibBuilderAppendN(XL6_StdlibTextBuilder* builder, const char* text, size_t len)
{
    if ( builder == NULL || text == NULL || !builder->ok ) {
        return;
    }
    if ( !xl6StdlibBuilderGrow(builder, len) ) {
        return;
    }
    memcpy(builder->data + builder->len, text, len);
    builder->len += len;
    builder->data[builder->len] = '\0';
}

static void xl6StdlibBuilderAppend(XL6_StdlibTextBuilder* builder, const char* text)
{
    xl6StdlibBuilderAppendN(builder, text, text != NULL ? strlen(text) : 0);
}

static void xl6StdlibBuilderAppendQuoted(XL6_StdlibTextBuilder* builder, const char* text)
{
    const unsigned char* p;

    xl6StdlibBuilderAppend(builder, "\"");
    if ( text != NULL ) {
        for ( p = (const unsigned char*)text; *p != '\0'; ++p ) {
            char tmp[7];
            if ( *p == '"' || *p == '\\' ) {
                tmp[0] = '\\';
                tmp[1] = (char)*p;
                tmp[2] = '\0';
                xl6StdlibBuilderAppend(builder, tmp);
            } else if ( *p < 0x20 ) {
                snprintf(tmp, sizeof(tmp), "\\x%02X", (unsigned)*p);
                xl6StdlibBuilderAppend(builder, tmp);
            } else {
                xl6StdlibBuilderAppendN(builder, (const char*)p, 1);
            }
        }
    }
    xl6StdlibBuilderAppend(builder, "\"");
}

static const XL6_StdlibFunc* xl6StdlibFindFunc(const char* ns, const char* name, size_t* outIndex)
{
    size_t i;

    for ( i = 0; i < XL6_STDLIB_FUNC_COUNT; ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibFuncs[i].ns, ns) &&
             xl6StdlibTextEquals(xl6StdlibFuncs[i].name, name) ) {
            if ( outIndex != NULL ) {
                *outIndex = i;
            }
            return &xl6StdlibFuncs[i];
        }
    }
    return NULL;
}

static const XL6_StdlibConst* xl6StdlibFindConst(const char* ns, const char* name)
{
    size_t i;

    for ( i = 0; i < XL6_STDLIB_CONST_COUNT; ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibConsts[i].ns, ns) &&
             xl6StdlibTextEquals(xl6StdlibConsts[i].name, name) ) {
            return &xl6StdlibConsts[i];
        }
    }
    return NULL;
}

static int xl6StdlibNamespaceExists(const char* ns)
{
    size_t i;

    for ( i = 0; i < XL6_STDLIB_FUNC_COUNT; ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibFuncs[i].ns, ns) ) {
            return 1;
        }
    }
    for ( i = 0; i < XL6_STDLIB_CONST_COUNT; ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibConsts[i].ns, ns) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6StdlibMarkFuncUsed(int* used, const char* ns, const char* name)
{
    size_t i;

    if ( used == NULL ) {
        return;
    }
    for ( i = 0; i < XL6_STDLIB_FUNC_COUNT; ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibFuncs[i].ns, ns) &&
             xl6StdlibTextEquals(xl6StdlibFuncs[i].name, name) ) {
            used[i] = 1;
        }
    }
}

static int xl6StdlibFuncUsed(const int* used, const char* ns, const char* name)
{
    size_t i;

    if ( used == NULL ) {
        return 0;
    }
    for ( i = 0; i < XL6_STDLIB_FUNC_COUNT; ++i ) {
        if ( used[i] &&
             xl6StdlibTextEquals(xl6StdlibFuncs[i].ns, ns) &&
             xl6StdlibTextEquals(xl6StdlibFuncs[i].name, name) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6StdlibApplyDeps(int* used)
{
    size_t index;

    if ( used == NULL ) {
        return;
    }

    /*
        round 在旧 MSVCRT/TCC 环境没有稳定符号。
        这里将它实现为 xlang helper，并自动注入 floor/ceil 依赖。
    */
    if ( xl6StdlibFindFunc("math", "round", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "math", "floor");
        xl6StdlibMarkFuncUsed(used, "math", "ceil");
    }
    if ( xl6StdlibFindFunc("math", "sign", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_math", "sign");
    }

    if ( xl6StdlibFindFunc("dict", "remove", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_value", "tableRemove");
    }

    if ( xl6StdlibFindFunc("", "print", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "print");
    }
    if ( xl6StdlibFindFunc("", "println", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "print");
    }

    if ( xl6StdlibFindFunc("string", "len", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strCharLen");
    }
    if ( xl6StdlibFindFunc("string", "byteLen", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strByteLen");
    }
    if ( xl6StdlibFindFunc("string", "compare", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strComp");
    }
    if ( xl6StdlibFindFunc("string", "equals", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strComp");
    }
    if ( xl6StdlibFindFunc("string", "contains", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "inStr");
    }
    if ( xl6StdlibFindFunc("string", "indexOf", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strIndexOf");
    }
    if ( xl6StdlibFindFunc("string", "lastIndexOf", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strLastIndexOf");
    }
    if ( xl6StdlibFindFunc("string", "count", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strCount");
    }
    if ( xl6StdlibFindFunc("string", "startsWith", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "stringStartsWith");
    }
    if ( xl6StdlibFindFunc("string", "endsWith", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "stringEndsWith");
    }
    if ( xl6StdlibFindFunc("string", "hasAny", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "checkStr");
    }
    if ( xl6StdlibFindFunc("string", "like", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strLike");
    }
    if ( xl6StdlibFindFunc("string", "approx", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strApprox");
    }
    if ( xl6StdlibFindFunc("string", "sim", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strSim");
    }
    if ( xl6StdlibFindFunc("string", "isUtf8", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "isUtf8");
    }
    if ( xl6StdlibFindFunc("string", "formatInt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "intFormat");
    }
    if ( xl6StdlibFindFunc("string", "formatUInt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "uintFormat");
    }
    if ( xl6StdlibFindFunc("string", "formatFloat", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "numFormat");
    }
    if ( xl6StdlibFuncUsed(used, "string", "format") ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "intFormat");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "uintFormat");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "numFormat");
    }
    if ( xl6StdlibFindFunc("string", "lower", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "lcase");
    }
    if ( xl6StdlibFindFunc("string", "upper", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "ucase");
    }
    if ( xl6StdlibFindFunc("string", "trim", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "trim");
    }
    if ( xl6StdlibFindFunc("string", "ltrim", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "ltrim");
    }
    if ( xl6StdlibFindFunc("string", "rtrim", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "rtrim");
    }
    if ( xl6StdlibFindFunc("string", "filter", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "filter");
    }
    if ( xl6StdlibFindFunc("string", "replace", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "replace");
    }
    if ( xl6StdlibFindFunc("string", "split", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "stringSplit");
    }
    if ( xl6StdlibFindFunc("string", "slice", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strSlice");
    }
    if ( xl6StdlibFindFunc("string", "left", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strLeft");
    }
    if ( xl6StdlibFindFunc("string", "right", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strRight");
    }
    if ( xl6StdlibFindFunc("string", "mid", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strMid");
    }
    if ( xl6StdlibFindFunc("string", "charAt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strCharAt");
    }
    if ( xl6StdlibFindFunc("string", "byteAt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strByteAt");
    }
    if ( xl6StdlibFindFunc("string", "join", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "stringJoin");
    }
    if ( xl6StdlibFindFunc("string", "repeat", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strRepeat");
    }
    if ( xl6StdlibFindFunc("string", "isEmpty", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strIsEmpty");
    }
    if ( xl6StdlibFindFunc("string", "isBlank", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strIsBlank");
    }
    if ( xl6StdlibFindFunc("string", "remove", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strRemove");
    }
    if ( xl6StdlibFindFunc("string", "removeAt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strRemoveAt");
    }
    if ( xl6StdlibFindFunc("string", "insert", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strInsert");
    }
    if ( xl6StdlibFindFunc("string", "padLeft", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strPadLeft");
    }
    if ( xl6StdlibFindFunc("string", "padRight", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strPadRight");
    }
    if ( xl6StdlibFindFunc("string", "padCenter", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strPadCenter");
    }
    if ( xl6StdlibFindFunc("string", "reverse", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "strReverse");
    }
    if ( xl6StdlibFindFunc("string", "splitLines", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "stringSplitLines");
    }
    if ( xl6StdlibFindFunc("string", "chars", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "stringChars");
    }
    if ( xl6StdlibFindFunc("string", "bytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "stringBytes");
    }

    if ( xl6StdlibFindFunc("time", "now", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "now");
    }
    if ( xl6StdlibFindFunc("time", "today", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "date");
    }
    if ( xl6StdlibFindFunc("time", "clock", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "time");
    }
    if ( xl6StdlibFindFunc("time", "fromTime", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timeSerial");
    }
    if ( xl6StdlibFindFunc("time", "fromDate", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateSerial");
    }
    if ( xl6StdlibFindFunc("time", "fromDateTime", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateTimeSerial");
    }
    if ( xl6StdlibFindFunc("time", "parse", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "strToTime");
    }
    if ( xl6StdlibFindFunc("time", "parsePattern", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timeParse");
    }
    if ( xl6StdlibFindFunc("time", "tryParse", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "tryStrToTime");
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "strToTime");
    }
    if ( xl6StdlibFindFunc("time", "tryParsePattern", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "tryTimeParse");
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timeParse");
    }
    if ( xl6StdlibFindFunc("time", "year", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "year");
    }
    if ( xl6StdlibFindFunc("time", "month", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "month");
    }
    if ( xl6StdlibFindFunc("time", "day", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "day");
    }
    if ( xl6StdlibFindFunc("time", "hour", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "hour");
    }
    if ( xl6StdlibFindFunc("time", "minute", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "minute");
    }
    if ( xl6StdlibFindFunc("time", "second", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "second");
    }
    if ( xl6StdlibFindFunc("time", "weekday", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "weekday");
    }
    if ( xl6StdlibFindFunc("time", "dayOfYear", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dayOfYear");
    }
    if ( xl6StdlibFindFunc("time", "quarter", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "quarter");
    }
    if ( xl6StdlibFindFunc("time", "isLeapYear", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "isLeapYear");
    }
    if ( xl6StdlibFindFunc("time", "daysInMonth", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "daysInMonth");
    }
    if ( xl6StdlibFindFunc("time", "daysInYear", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "daysInYear");
    }
    if ( xl6StdlibFindFunc("time", "datePart", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "datePart");
    }
    if ( xl6StdlibFindFunc("time", "timePart", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timePart");
    }
    if ( xl6StdlibFindFunc("time", "isSameDay", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "isSameDay");
    }
    if ( xl6StdlibFindFunc("time", "isSameMonth", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "isSameMonth");
    }
    if ( xl6StdlibFindFunc("time", "isSameYear", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "isSameYear");
    }
    if ( xl6StdlibFindFunc("time", "inRange", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "inRange");
    }
    if ( xl6StdlibFindFunc("time", "addYears", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateAdd");
    }
    if ( xl6StdlibFindFunc("time", "addMonths", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateAdd");
    }
    if ( xl6StdlibFindFunc("time", "addDays", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateAdd");
    }
    if ( xl6StdlibFindFunc("time", "addHours", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateAdd");
    }
    if ( xl6StdlibFindFunc("time", "addMinutes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateAdd");
    }
    if ( xl6StdlibFindFunc("time", "addSeconds", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateAdd");
    }
    if ( xl6StdlibFindFunc("time", "addWeeks", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateAdd");
    }
    if ( xl6StdlibFindFunc("time", "addQuarters", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateAdd");
    }
    if ( xl6StdlibFindFunc("time", "diffYears", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateDiff");
    }
    if ( xl6StdlibFindFunc("time", "diffMonths", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateDiff");
    }
    if ( xl6StdlibFindFunc("time", "diffDays", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateDiff");
    }
    if ( xl6StdlibFindFunc("time", "diffHours", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateDiff");
    }
    if ( xl6StdlibFindFunc("time", "diffMinutes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateDiff");
    }
    if ( xl6StdlibFindFunc("time", "diffSeconds", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateDiff");
    }
    if ( xl6StdlibFindFunc("time", "diffWeeks", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateDiff");
    }
    if ( xl6StdlibFindFunc("time", "diffQuarters", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "dateDiff");
    }
    if ( xl6StdlibFindFunc("time", "timer", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timer");
    }
    if ( xl6StdlibFindFunc("time", "sleep", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "sleep");
    }
    if ( xl6StdlibFindFunc("time", "nowUTC", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "nowUTC");
    }
    if ( xl6StdlibFindFunc("time", "timezoneOffset", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timezoneOffset");
    }
    if ( xl6StdlibFindFunc("time", "rangeOverlap", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "rangeOverlap");
    }
    if ( xl6StdlibFindFunc("time", "approx", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "approx");
    }
    if ( xl6StdlibFindFunc("time", "toUnix", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "toUnix");
    }
    if ( xl6StdlibFindFunc("time", "fromUnix", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "fromUnix");
    }
    if ( xl6StdlibFindFunc("time", "utcToLocal", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "utcToLocal");
    }
    if ( xl6StdlibFindFunc("time", "localToUTC", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "localToUTC");
    }
    if ( xl6StdlibFindFunc("time", "firstDayOfMonth", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "firstDayOfMonth");
    }
    if ( xl6StdlibFindFunc("time", "lastDayOfMonth", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "lastDayOfMonth");
    }
    if ( xl6StdlibFindFunc("time", "firstDayOfYear", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "firstDayOfYear");
    }
    if ( xl6StdlibFindFunc("time", "lastDayOfYear", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "lastDayOfYear");
    }
    if ( xl6StdlibFindFunc("time", "firstDayOfWeek", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "firstDayOfWeek");
    }
    if ( xl6StdlibFindFunc("time", "lastDayOfWeek", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "lastDayOfWeek");
    }
    if ( xl6StdlibFindFunc("time", "weekOfYear", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "weekOfYear");
    }
    if ( xl6StdlibFindFunc("time", "weekOfMonth", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "weekOfMonth");
    }
    if ( xl6StdlibFindFunc("time", "format", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timeToStr");
    }
    if ( xl6StdlibFindFunc("time", "formatPattern", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timeFormat");
    }
    if ( xl6StdlibFindFunc("time", "dateString", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timeToStr");
    }
    if ( xl6StdlibFindFunc("time", "timeString", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "timeToStr");
    }
    if ( xl6StdlibFindFunc("time", "relative", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_time", "relativeTime");
    }

    if ( xl6StdlibFindFunc("random", "u32", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_random", "rand32");
    }
    if ( xl6StdlibFindFunc("random", "u64", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_random", "rand64");
    }
    if ( xl6StdlibFindFunc("random", "range", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_random", "range");
    }
    if ( xl6StdlibFindFunc("random", "seed", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_random", "seedThread");
    }
    if ( xl6StdlibFindFunc("random", "string", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_random", "string");
    }

    if ( xl6StdlibFindFunc("path", "exists", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "exists");
    }
    if ( xl6StdlibFindFunc("path", "isAbs", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "isAbs");
    }
    if ( xl6StdlibFindFunc("path", "nameExt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "getNameExt");
    }
    if ( xl6StdlibFindFunc("path", "name", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "getName");
    }
    if ( xl6StdlibFindFunc("path", "ext", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "getExt");
    }
    if ( xl6StdlibFindFunc("path", "dir", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "getDir");
    }
    if ( xl6StdlibFindFunc("path", "join", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "pathJoin");
    }
    if ( xl6StdlibFindFunc("path", "random", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "random");
    }
    if ( xl6StdlibFindFunc("path", "sep", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "sep");
    }
    if ( xl6StdlibFindFunc("path", "normalize", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "normalize");
    }
    if ( xl6StdlibFindFunc("path", "abs", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "abs");
    }
    if ( xl6StdlibFindFunc("path", "rel", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "rel");
    }
    if ( xl6StdlibFindFunc("path", "cwd", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "cwd");
    }
    if ( xl6StdlibFindFunc("path", "setCwd", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "setCwd");
    }
    if ( xl6StdlibFindFunc("path", "home", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "home");
    }
    if ( xl6StdlibFindFunc("path", "temp", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "temp");
    }
    if ( xl6StdlibFindFunc("path", "appDir", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "appDir");
    }
    if ( xl6StdlibFindFunc("path", "withExt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "withExt");
    }
    if ( xl6StdlibFindFunc("path", "withName", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "withName");
    }

    if ( xl6StdlibFindFunc("file", "exists", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "exists");
    }
    if ( xl6StdlibFindFunc("file", "size", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "getSize");
    }
    if ( xl6StdlibFindFunc("file", "setSize", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "setSize");
    }
    if ( xl6StdlibFindFunc("file", "attr", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "getAttr");
    }
    if ( xl6StdlibFindFunc("file", "setAttr", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "setAttr");
    }
    if ( xl6StdlibFindFunc("file", "accessTime", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "getAccessTime");
    }
    if ( xl6StdlibFindFunc("file", "changeTime", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "getChangeTime");
    }
    if ( xl6StdlibFindFunc("file", "readText", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "readTextStrict");
    }
    if ( xl6StdlibFindFunc("file", "readBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "fileReadBytes");
    }
    if ( xl6StdlibFindFunc("file", "writeText", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "writeAll");
    }
    if ( xl6StdlibFindFunc("file", "appendText", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "append");
    }
    if ( xl6StdlibFindFunc("file", "writeBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "fileWriteBytes");
    }
    if ( xl6StdlibFindFunc("file", "appendBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "fileAppendBytes");
    }
    if ( xl6StdlibFindFunc("file", "delete", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "delete");
    }
    if ( xl6StdlibFindFunc("file", "copy", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "copy");
    }
    if ( xl6StdlibFindFunc("file", "move", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "move");
    }
    if ( xl6StdlibFindFunc("file", "touch", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "touch");
    }
    if ( xl6StdlibFindFunc("file", "rename", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "rename");
    }
    if ( xl6StdlibFindFunc("file", "replace", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "replace");
    }
    if ( xl6StdlibFindFunc("file", "writeTextAtomic", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "writeAllAtomic");
    }
    if ( xl6StdlibFindFunc("file", "info", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_path", "exists");
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "exists");
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "exists");
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "getSize");
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "getAttr");
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "getAccessTime");
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "getChangeTime");
    }
    if ( xl6StdlibFindFunc("dir", "exists", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "exists");
    }
    if ( xl6StdlibFindFunc("dir", "create", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "create");
    }
    if ( xl6StdlibFindFunc("dir", "createAll", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "createAll");
    }
    if ( xl6StdlibFindFunc("dir", "copy", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "exists");
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "copy");
    }
    if ( xl6StdlibFindFunc("dir", "move", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "exists");
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "move");
    }
    if ( xl6StdlibFindFunc("dir", "delete", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "exists");
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "delete");
    }
    if ( xl6StdlibFindFunc("dir", "list", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "dirList");
    }
    if ( xl6StdlibFindFunc("dir", "files", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "dirList");
    }
    if ( xl6StdlibFindFunc("dir", "dirs", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "dirList");
    }
    if ( xl6StdlibFindFunc("dir", "empty", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "isEmpty");
    }
    if ( xl6StdlibFindFunc("dir", "count", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "count");
    }
    if ( xl6StdlibFindFunc("dir", "size", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "size");
    }
    if ( xl6StdlibFindFunc("dir", "clean", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "clean");
    }
    if ( xl6StdlibFindFunc("dir", "ensureEmpty", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_dir", "ensureEmpty");
    }
    if ( xl6StdlibFindFunc("dir", "glob", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "dirGlob");
    }
    if ( xl6StdlibFindFunc("dir", "entries", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "dirEntries");
    }
    if ( xl6StdlibFindFunc("dir", "match", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "dirEntries");
    }
    if ( xl6StdlibFindFunc("dir", "walk", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "dirEntries");
    }

    if ( xl6StdlibFindFunc("json", "parse", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_json", "parse");
    }
    if ( xl6StdlibFindFunc("json", "valid", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_json", "valid");
    }
    if ( xl6StdlibFindFunc("json", "read", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_json", "parseFile");
    }
    if ( xl6StdlibFindFunc("json", "stringify", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_json", "stringify");
    }
    if ( xl6StdlibFindFunc("json", "write", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_json", "writeFile");
    }

    if ( xl6StdlibFindFunc("xson", "parse", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_xson", "parse");
    }
    if ( xl6StdlibFindFunc("xson", "valid", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_xson", "valid");
    }
    if ( xl6StdlibFindFunc("xson", "read", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_xson", "parseFile");
    }
    if ( xl6StdlibFindFunc("xson", "stringify", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_xson", "stringify");
    }
    if ( xl6StdlibFindFunc("xson", "write", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_xson", "writeFile");
    }

    if ( xl6StdlibFindFunc("codec", "hexEncode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "hexEncode");
    }
    if ( xl6StdlibFindFunc("codec", "hexDecode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "hexDecode");
    }
    if ( xl6StdlibFindFunc("codec", "base64Encode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "base64Encode");
    }
    if ( xl6StdlibFindFunc("codec", "base64Decode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "base64Decode");
    }
    if ( xl6StdlibFindFunc("codec", "hexEncodeBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "hexEncodeBytes");
    }
    if ( xl6StdlibFindFunc("codec", "hexDecodeBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "bufferFromHex");
    }
    if ( xl6StdlibFindFunc("codec", "base64EncodeBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "base64EncodeBytes");
    }
    if ( xl6StdlibFindFunc("codec", "base64DecodeBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "bufferFromBase64");
    }
    if ( xl6StdlibFindFunc("codec", "percentEncode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "percentEncode");
    }
    if ( xl6StdlibFindFunc("codec", "percentDecode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "percentDecode");
    }
    if ( xl6StdlibFindFunc("codec", "formUrlEncode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "formUrlEncode");
    }
    if ( xl6StdlibFindFunc("codec", "formUrlDecode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_codec", "formUrlDecode");
    }

    if ( xl6StdlibFindFunc("encoding", "charSize", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "encodingCharSize");
    }
    if ( xl6StdlibFindFunc("encoding", "isUtf8", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "encodingIsUtf8Text");
    }
    if ( xl6StdlibFindFunc("encoding", "isUtf8Bytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "encodingIsUtf8Bytes");
    }
    if ( xl6StdlibFindFunc("encoding", "detect", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "encodingDetectText");
    }
    if ( xl6StdlibFindFunc("encoding", "detectBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "encodingDetectBytes");
    }
    if ( xl6StdlibFindFunc("encoding", "toBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "encodingToBytes");
    }
    if ( xl6StdlibFindFunc("encoding", "fromBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "encodingFromBytes");
    }
    if ( xl6StdlibFindFunc("encoding", "convertBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "encodingConvertBytes");
    }
    if ( xl6StdlibFindFunc("encoding", "name", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "encodingName");
    }

    if ( xl6StdlibFindFunc("hash", "u32", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_hash", "u32");
    }
    if ( xl6StdlibFindFunc("hash", "u32WithSeed", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_hash", "u32WithSeed");
    }
    if ( xl6StdlibFindFunc("hash", "u64", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_hash", "u64");
    }
    if ( xl6StdlibFindFunc("hash", "u64WithSeed", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_hash", "u64WithSeed");
    }
    if ( xl6StdlibFindFunc("hash", "u64Micro", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_hash", "u64Micro");
    }
    if ( xl6StdlibFindFunc("hash", "u64MicroWithSeed", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_hash", "u64MicroWithSeed");
    }
    if ( xl6StdlibFindFunc("hash", "u64Nano", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_hash", "u64Nano");
    }
    if ( xl6StdlibFindFunc("hash", "u64NanoWithSeed", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xrt_hash", "u64NanoWithSeed");
    }

    if ( xl6StdlibFindFunc("crypto", "sha1", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "sha1Hex");
    }
    if ( xl6StdlibFindFunc("crypto", "sha256", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "sha256Hex");
    }
    if ( xl6StdlibFindFunc("crypto", "sha384", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "sha384Hex");
    }
    if ( xl6StdlibFindFunc("crypto", "sha512", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "sha512Hex");
    }
    if ( xl6StdlibFindFunc("crypto", "hmacSha256", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "hmacSha256Hex");
    }
    if ( xl6StdlibFindFunc("crypto", "hmacSha384", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "hmacSha384Hex");
    }
    if ( xl6StdlibFindFunc("crypto", "hmacSha512", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "hmacSha512Hex");
    }
    if ( xl6StdlibFindFunc("crypto", "chacha20Poly1305Encrypt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "chacha20Poly1305EncryptHex");
    }
    if ( xl6StdlibFindFunc("crypto", "chacha20Poly1305Decrypt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "chacha20Poly1305DecryptText");
    }
    if ( xl6StdlibFindFunc("crypto", "aes128GcmEncrypt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "aes128GcmEncryptHex");
    }
    if ( xl6StdlibFindFunc("crypto", "aes128GcmDecrypt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "aes128GcmDecryptText");
    }
    if ( xl6StdlibFindFunc("crypto", "aes256GcmEncrypt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "aes256GcmEncryptHex");
    }
    if ( xl6StdlibFindFunc("crypto", "aes256GcmDecrypt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "aes256GcmDecryptText");
    }
    if ( xl6StdlibFindFunc("crypto", "hkdfSha256", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "hkdfSha256Hex");
    }
    if ( xl6StdlibFindFunc("crypto", "hkdfSha384", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "hkdfSha384Hex");
    }
    if ( xl6StdlibFindFunc("crypto", "randomHex", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "randomHex");
    }
    if ( xl6StdlibFindFunc("crypto", "chacha20Encrypt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "chacha20EncryptHex");
    }
    if ( xl6StdlibFindFunc("crypto", "chacha20Decrypt", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "chacha20DecryptText");
    }
    if ( xl6StdlibFindFunc("crypto", "ed25519Keypair", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "ed25519Keypair");
    }
    if ( xl6StdlibFindFunc("crypto", "ed25519PublicKey", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "ed25519PublicKeyHex");
    }
    if ( xl6StdlibFindFunc("crypto", "ed25519Sign", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "ed25519SignHex");
    }
    if ( xl6StdlibFindFunc("crypto", "ed25519Verify", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "ed25519Verify");
    }
    if ( xl6StdlibFindFunc("crypto", "x25519Keypair", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "x25519Keypair");
    }
    if ( xl6StdlibFindFunc("crypto", "x25519SharedSecret", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "x25519SharedSecretHex");
    }
    if ( xl6StdlibFindFunc("crypto", "x448Keypair", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "x448Keypair");
    }
    if ( xl6StdlibFindFunc("crypto", "x448SharedSecret", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "x448SharedSecretHex");
    }
    if ( xl6StdlibFindFunc("crypto", "rsaPssVerify", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "rsaPssVerify");
    }
    if ( xl6StdlibFindFunc("crypto", "rsaPkcs1Verify", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "rsaPkcs1Verify");
    }

    if ( xl6StdlibFindFunc("id", "xid", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_id", "makeXIDS");
    }

    if ( xl6StdlibFindFunc("net", "localIP", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_net", "localIP");
    }
    if ( xl6StdlibFindFunc("net", "localMAC", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_net", "localMAC");
    }
    if ( xl6StdlibFindFunc("net", "localName", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_net", "localName");
    }
    if ( xl6StdlibFindFunc("tcp", "connect", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_tcp", "connect");
    }
    if ( xl6StdlibFindFunc("tcp", "connectAsync", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_tcp", "connectAsync");
    }
    if ( xl6StdlibFindFunc("tcp", "listen", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_tcp", "listen");
    }
    if ( xl6StdlibFindFunc("udp", "bind", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_udp", "bind");
    }

    if ( xl6StdlibFindFunc("url", "valid", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlFlag");
    }
    if ( xl6StdlibFindFunc("url", "isAbsolute", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlFlag");
    }
    if ( xl6StdlibFindFunc("url", "isSecure", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlFlag");
    }
    if ( xl6StdlibFindFunc("url", "port", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlPort");
    }
    if ( xl6StdlibFindFunc("url", "scheme", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlPart");
    }
    if ( xl6StdlibFindFunc("url", "authority", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlPart");
    }
    if ( xl6StdlibFindFunc("url", "userInfo", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlPart");
    }
    if ( xl6StdlibFindFunc("url", "host", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlPart");
    }
    if ( xl6StdlibFindFunc("url", "path", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlPart");
    }
    if ( xl6StdlibFindFunc("url", "query", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlPart");
    }
    if ( xl6StdlibFindFunc("url", "fragment", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlPart");
    }
    if ( xl6StdlibFindFunc("url", "target", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlPart");
    }
    if ( xl6StdlibFindFunc("url", "resolve", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlResolve");
    }
    if ( xl6StdlibFindFunc("url", "normalizePath", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "urlNormalizePath");
    }

    if ( xl6StdlibFindFunc("query", "encode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "queryEncode");
    }
    if ( xl6StdlibFindFunc("query", "decode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "queryDecode");
    }
    if ( xl6StdlibFindFunc("query", "valid", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "queryValid");
    }
    if ( xl6StdlibFindFunc("query", "pair", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "queryPair");
    }
    if ( xl6StdlibFindFunc("query", "append", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "queryAppend");
    }

    if ( xl6StdlibFindFunc("regex", "compile", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
    }
    if ( xl6StdlibFindFunc("regex", "isMatch", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "regexIsMatch");
    }
    if ( xl6StdlibFindFunc("regex", "fullMatch", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "regexFullMatch");
    }
    if ( xl6StdlibFindFunc("regex", "find", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "regexFind");
    }
    if ( xl6StdlibFindFunc("regex", "match", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "regexMatch");
    }
    if ( xl6StdlibFindFunc("regex", "set", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "regexSetCreate");
    }

    if ( xl6StdlibFindFunc("process", "exec", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "processExec");
    }
    if ( xl6StdlibFindFunc("process", "success", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "processSuccess");
    }
    if ( xl6StdlibFindFunc("process", "exitCode", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "processExitCode");
    }
    if ( xl6StdlibFindFunc("process", "run", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "processRun");
    }
    if ( xl6StdlibFindFunc("process", "runFile", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "processRunFile");
    }
    if ( xl6StdlibFindFunc("process", "pipeline", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "processPipeline");
    }

    if ( xl6StdlibFindFunc("log", "default", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefault");
    }
    if ( xl6StdlibFindFunc("log", "setLevel", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultSetLevel");
    }
    if ( xl6StdlibFindFunc("log", "getLevel", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultGetLevel");
    }
    if ( xl6StdlibFindFunc("log", "levelName", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logLevelName");
    }
    if ( xl6StdlibFindFunc("log", "addConsole", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultAddConsole");
    }
    if ( xl6StdlibFindFunc("log", "addFile", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultAddFile");
    }
    if ( xl6StdlibFindFunc("log", "addRollingFile", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultAddRollingFile");
    }
    if ( xl6StdlibFindFunc("log", "write", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultWrite");
    }
    if ( xl6StdlibFindFunc("log", "trace", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultWrite");
    }
    if ( xl6StdlibFindFunc("log", "debug", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultWrite");
    }
    if ( xl6StdlibFindFunc("log", "info", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultWrite");
    }
    if ( xl6StdlibFindFunc("log", "warn", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultWrite");
    }
    if ( xl6StdlibFindFunc("log", "error", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultWrite");
    }
    if ( xl6StdlibFindFunc("log", "fatal", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultWrite");
    }
    if ( xl6StdlibFindFunc("log", "flush", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "logDefaultFlush");
    }

    if ( xl6StdlibFindFunc("template", "renderFile", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "template", "render");
        xl6StdlibMarkFuncUsed(used, "file", "readText");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "readTextStrict");
    }

    if ( xl6StdlibFindFunc("http.server", "create", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerCreate");
    }
    if ( xl6StdlibFindFunc("http.server", "createTls", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerCreateTls");
    }
    if ( xl6StdlibFindFunc("http.server", "app", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppCreate");
    }
    if ( xl6StdlibFindFunc("http.server", "destroyApp", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppDestroy");
    }
    if ( xl6StdlibFindFunc("http.server", "appRoute", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppRoute");
    }
    if ( xl6StdlibFindFunc("http.server", "appRouteRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppRouteRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "appGet", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppGet");
    }
    if ( xl6StdlibFindFunc("http.server", "appPost", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppPost");
    }
    if ( xl6StdlibFindFunc("http.server", "appAny", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppAny");
    }
    if ( xl6StdlibFindFunc("http.server", "appGetRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppGetRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "appPostRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppPostRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "appAnyRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppAnyRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "appRouteStreamRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppRouteStreamRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "appPostStreamRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppPostStreamRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "appAnyStreamRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppAnyStreamRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "appStatic", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppStatic");
    }
    if ( xl6StdlibFindFunc("http.server", "appStaticEx", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppStaticEx");
    }
    if ( xl6StdlibFindFunc("http.server", "appError", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpAppError");
    }
    if ( xl6StdlibFindFunc("http.server", "setApp", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerSetApp");
    }
    if ( xl6StdlibFindFunc("http.server", "reloadApp", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerReloadApp");
    }
    if ( xl6StdlibFindFunc("http.server", "host", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerHost");
    }
    if ( xl6StdlibFindFunc("http.server", "removeHost", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerRemoveHost");
    }
    if ( xl6StdlibFindFunc("http.server", "tlsHost", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerTlsHost");
    }
    if ( xl6StdlibFindFunc("http.server", "removeTlsHost", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerRemoveTlsHost");
    }
    if ( xl6StdlibFindFunc("http.server", "error", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerError");
    }
    if ( xl6StdlibFindFunc("http.server", "route", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerRoute");
    }
    if ( xl6StdlibFindFunc("http.server", "routeRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerRouteRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "get", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerGet");
    }
    if ( xl6StdlibFindFunc("http.server", "post", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerPost");
    }
    if ( xl6StdlibFindFunc("http.server", "any", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerAny");
    }
    if ( xl6StdlibFindFunc("http.server", "getRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerGetRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "postRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerPostRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "anyRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerAnyRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "routeStreamRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerRouteStreamRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "postStreamRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerPostStreamRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "anyStreamRequest", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerAnyStreamRequest");
    }
    if ( xl6StdlibFindFunc("http.server", "static", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerStatic");
    }
    if ( xl6StdlibFindFunc("http.server", "staticEx", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerStaticEx");
    }
    if ( xl6StdlibFindFunc("http.server", "start", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerStart");
    }
    if ( xl6StdlibFindFunc("http.server", "port", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerPort");
    }
    if ( xl6StdlibFindFunc("http.server", "requestCount", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerRequestCount");
    }
    if ( xl6StdlibFindFunc("http.server", "connectionCount", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerConnectionCount");
    }
    if ( xl6StdlibFindFunc("http.server", "stop", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerStop");
    }
    if ( xl6StdlibFindFunc("http.server", "drain", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerDrain");
    }
    if ( xl6StdlibFindFunc("http.server", "destroy", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerDestroy");
    }
    if ( xl6StdlibFindFunc("http.server", "mimeByPath", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpMimeByPath");
    }
    if ( xl6StdlibFindFunc("http.server", "chunkText", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpBodyChunkText");
    }
    if ( xl6StdlibFindFunc("http.server", "chunkBytes", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpBodyChunkBytes");
    }
    if ( xl6StdlibFindFunc("http", "get", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "http", "request");
    }
    if ( xl6StdlibFindFunc("http", "post", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "http", "request");
    }
    if ( xl6StdlibFindFunc("http", "request", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
    }
}

static int xl6StdlibAppendPath(char* out, size_t outSize, const char* text)
{
    size_t oldLen;
    size_t addLen;

    if ( out == NULL || outSize == 0 || text == NULL ) {
        return 0;
    }
    oldLen = strlen(out);
    addLen = strlen(text);
    if ( oldLen + addLen + 1 > outSize ) {
        return 0;
    }
    memcpy(out + oldLen, text, addLen + 1);
    return 1;
}

static int xl6StdlibReceiverPath(const XL6_AstNode* node, char* out, size_t outSize)
{
    if ( out == NULL || outSize == 0 || node == NULL ) {
        return 0;
    }
    out[0] = '\0';
    if ( node->kind == XL6_AST_NAME ) {
        return xl6StdlibAppendPath(out, outSize, node->text);
    }
    if ( node->kind == XL6_AST_MEMBER && node->childCount > 0 && node->children[0] != NULL ) {
        if ( !xl6StdlibReceiverPath(node->children[0], out, outSize) ) {
            return 0;
        }
        if ( !xl6StdlibAppendPath(out, outSize, ".") ) {
            return 0;
        }
        return xl6StdlibAppendPath(out, outSize, node->text);
    }
    return 0;
}

static void xl6StdlibRewriteMemberToName(XL6_AstNode* node, const char* name)
{
    size_t i;

    if ( node == NULL || name == NULL ) {
        return;
    }

    /* 释放原成员访问结果，保留节点位置用于诊断。 */
    for ( i = 0; i < node->childCount; ++i ) {
        xl6AstFree(node->children[i]);
    }
    xrtFree(node->children);
    xrtFree(node->text);

    node->kind = XL6_AST_NAME;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    node->text = xl6StdlibDupText(name);
}

static void xl6StdlibRewriteMemberToLiteral(XL6_AstNode* node, XL6_AstKind kind, const char* value)
{
    size_t i;
    int negativeInt;

    if ( node == NULL || value == NULL ) {
        return;
    }
    negativeInt = kind == XL6_AST_INT && value[0] == '-' && value[1] != '\0';
    for ( i = 0; i < node->childCount; ++i ) {
        xl6AstFree(node->children[i]);
    }
    xrtFree(node->children);
    xrtFree(node->text);

    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    if ( negativeInt ) {
        XL6_AstNode* child = xl6AstNewText(XL6_AST_INT, value + 1, strlen(value + 1), NULL);
        if ( child == NULL ) {
            node->kind = kind;
            node->text = NULL;
            return;
        }
        child->line = node->line;
        child->column = node->column;
        node->kind = XL6_AST_UNARY;
        node->text = xl6StdlibDupText("-");
        xl6AstAddChild(node, child);
        return;
    }
    node->kind = kind;
    node->text = xl6StdlibDupText(value);
}

typedef struct XL6_StdlibNameScope {
    const struct XL6_StdlibNameScope* parent;
    const char** names;
    size_t count;
    size_t capacity;
} XL6_StdlibNameScope;

static void xl6StdlibScopeInit(XL6_StdlibNameScope* scope, const XL6_StdlibNameScope* parent)
{
    if ( scope == NULL ) {
        return;
    }
    memset(scope, 0, sizeof(*scope));
    scope->parent = parent;
}

static void xl6StdlibScopeFree(XL6_StdlibNameScope* scope)
{
    if ( scope == NULL ) {
        return;
    }
    xrtFree(scope->names);
    memset(scope, 0, sizeof(*scope));
}

static int xl6StdlibScopeContains(const XL6_StdlibNameScope* scope, const char* name)
{
    size_t i;

    for ( ; scope != NULL; scope = scope->parent ) {
        for ( i = 0; i < scope->count; ++i ) {
            if ( xl6StdlibTextEquals(scope->names[i], name) ) {
                return 1;
            }
        }
    }
    return 0;
}

static int xl6StdlibScopeAdd(XL6_StdlibNameScope* scope, const char* name)
{
    const char** names;
    size_t capacity;

    if ( scope == NULL || name == NULL || name[0] == '\0' || xl6StdlibScopeContains(scope, name) ) {
        return 1;
    }
    if ( scope->count == scope->capacity ) {
        capacity = scope->capacity == 0 ? 8 : scope->capacity * 2;
        names = (const char**)xrtRealloc(scope->names, capacity * sizeof(const char*));
        if ( names == NULL ) {
            return 0;
        }
        scope->names = names;
        scope->capacity = capacity;
    }
    scope->names[scope->count++] = name;
    return 1;
}

static int xl6StdlibScopeAddParams(XL6_StdlibNameScope* scope, const XL6_AstNode* params)
{
    size_t i;

    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && !xl6StdlibScopeAdd(scope, param->text) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6StdlibScopeAddDecl(XL6_StdlibNameScope* scope, const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 1;
    }
    if ( node->kind == XL6_AST_VAR_DECL || node->kind == XL6_AST_CONST_DECL ) {
        return xl6StdlibScopeAdd(scope, node->text);
    }
    if ( node->kind == XL6_AST_MULTI_VAR_DECL && node->childCount > 0 ) {
        const XL6_AstNode* names = node->children[0];
        for ( i = 0; names != NULL && i < names->childCount; ++i ) {
            if ( names->children[i] != NULL && !xl6StdlibScopeAdd(scope, names->children[i]->text) ) {
                return 0;
            }
        }
    }
    return 1;
}

static const char* xl6StdlibReceiverRootName(const XL6_AstNode* node)
{
    while ( node != NULL && (node->kind == XL6_AST_MEMBER || node->kind == XL6_AST_SAFE_MEMBER) && node->childCount > 0 ) {
        node = node->children[0];
    }
    return node != NULL && node->kind == XL6_AST_NAME ? node->text : NULL;
}

static int xl6StdlibRewriteRefsScoped(
    XL6_AstNode* node,
    int* used,
    XL6_DiagList* diags,
    XL6_StdlibNameScope* scope);

static int xl6StdlibRewriteCallable(
    XL6_AstNode* node,
    const XL6_AstNode* params,
    XL6_AstNode* body,
    int* used,
    XL6_DiagList* diags,
    XL6_StdlibNameScope* scope)
{
    XL6_StdlibNameScope functionScope;
    size_t i;
    int ok;

    xl6StdlibScopeInit(&functionScope, scope);
    ok = xl6StdlibScopeAddParams(&functionScope, params);
    for ( i = 0; ok && i < node->childCount; ++i ) {
        if ( node->children[i] != body &&
             !xl6StdlibRewriteRefsScoped(node->children[i], used, diags, &functionScope) ) {
            ok = 0;
        }
    }
    if ( ok && body != NULL ) {
        ok = xl6StdlibRewriteRefsScoped(body, used, diags, &functionScope);
    }
    xl6StdlibScopeFree(&functionScope);
    return ok;
}

static int xl6StdlibRewriteRefsScoped(
    XL6_AstNode* node,
    int* used,
    XL6_DiagList* diags,
    XL6_StdlibNameScope* scope)
{
    char ns[128];
    const char* rootName;
    size_t index;
    size_t i;
    int ok;

    if ( node == NULL ) {
        return 1;
    }

    if ( node->kind == XL6_AST_MODULE || node->kind == XL6_AST_BLOCK ) {
        XL6_StdlibNameScope blockScope;

        xl6StdlibScopeInit(&blockScope, scope);
        ok = 1;
        if ( node->kind == XL6_AST_MODULE ) {
            for ( i = 0; i < node->childCount; ++i ) {
                const XL6_AstNode* item = node->children[i];
                if ( !xl6StdlibScopeAddDecl(&blockScope, item) ) {
                    ok = 0;
                }
                if ( item != NULL &&
                     (item->kind == XL6_AST_FUNC || item->kind == XL6_AST_ASYNC_FUNC ||
                      item->kind == XL6_AST_EXTERN_FUNC || item->kind == XL6_AST_EXPORT_FUNC) &&
                     item->childCount > 0 && item->children[0] != NULL &&
                     !xl6StdlibScopeAdd(&blockScope, item->children[0]->text) ) {
                    ok = 0;
                }
            }
        }
        for ( i = 0; i < node->childCount; ++i ) {
            if ( !xl6StdlibRewriteRefsScoped(node->children[i], used, diags, &blockScope) ||
                 (node->kind == XL6_AST_BLOCK && !xl6StdlibScopeAddDecl(&blockScope, node->children[i])) ) {
                ok = 0;
            }
        }
        xl6StdlibScopeFree(&blockScope);
        return ok;
    }

    if ( node->kind == XL6_AST_FUNC || node->kind == XL6_AST_ASYNC_FUNC ||
         node->kind == XL6_AST_EXPORT_FUNC || node->kind == XL6_AST_GET || node->kind == XL6_AST_LET ) {
        const XL6_AstNode* params = node->childCount > 1 ? node->children[1] : NULL;
        XL6_AstNode* body = node->childCount > 3 ? node->children[3] : NULL;
        return xl6StdlibRewriteCallable(node, params, body, used, diags, scope);
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        XL6_AstNode* body = node->childCount > 2 ? node->children[2] : NULL;
        return xl6StdlibRewriteCallable(node, params, body, used, diags, scope);
    }
    if ( node->kind == XL6_AST_CREATE ) {
        const XL6_AstNode* params = node->childCount > 0 ? node->children[0] : NULL;
        XL6_AstNode* body = node->childCount > 1 ? node->children[1] : NULL;
        return xl6StdlibRewriteCallable(node, params, body, used, diags, scope);
    }
    if ( node->kind == XL6_AST_CLASS && node->childCount > 1 ) {
        const XL6_AstNode* body = node->children[1];
        XL6_StdlibNameScope classScope;

        xl6StdlibScopeInit(&classScope, scope);
        ok = 1;
        for ( i = 0; body != NULL && i < body->childCount; ++i ) {
            const XL6_AstNode* member = body->children[i];
            if ( member != NULL && member->kind == XL6_AST_FIELD &&
                 !xl6StdlibScopeAdd(&classScope, member->text) ) {
                ok = 0;
            }
        }
        for ( i = 0; ok && i < node->childCount; ++i ) {
            if ( node->children[i] != body &&
                 !xl6StdlibRewriteRefsScoped(node->children[i], used, diags, &classScope) ) {
                ok = 0;
            }
        }
        for ( i = 0; ok && body != NULL && i < body->childCount; ++i ) {
            if ( !xl6StdlibRewriteRefsScoped(body->children[i], used, diags, &classScope) ) {
                ok = 0;
            }
        }
        xl6StdlibScopeFree(&classScope);
        return ok;
    }
    if ( node->kind == XL6_AST_FOR && node->childCount > 2 ) {
        const XL6_AstNode* targets = node->children[0];
        XL6_StdlibNameScope loopScope;

        ok = xl6StdlibRewriteRefsScoped(node->children[1], used, diags, scope);
        xl6StdlibScopeInit(&loopScope, scope);
        for ( i = 0; ok && targets != NULL && i < targets->childCount; ++i ) {
            if ( targets->children[i] != NULL && !xl6StdlibScopeAdd(&loopScope, targets->children[i]->text) ) {
                ok = 0;
            }
        }
        if ( ok ) {
            ok = xl6StdlibRewriteRefsScoped(node->children[2], used, diags, &loopScope);
        }
        xl6StdlibScopeFree(&loopScope);
        return ok;
    }
    if ( node->kind == XL6_AST_CATCH && node->childCount > 1 ) {
        XL6_StdlibNameScope catchScope;

        xl6StdlibScopeInit(&catchScope, scope);
        ok = xl6StdlibScopeAdd(&catchScope, node->children[0] != NULL ? node->children[0]->text : NULL) &&
             xl6StdlibRewriteRefsScoped(node->children[1], used, diags, &catchScope);
        xl6StdlibScopeFree(&catchScope);
        return ok;
    }

    /* 全局标准库函数：print(...) -> __xl_std_print(...)。 */
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME ) {
        XL6_AstNode* callee = node->children[0];
        const XL6_StdlibFunc* func = xl6StdlibScopeContains(scope, callee->text) ?
                                    NULL : xl6StdlibFindFunc("", callee->text, &index);
        if ( func != NULL ) {
            xl6StdlibMarkFuncUsed(used, "", callee->text);
            xrtFree(callee->text);
            callee->text = xl6StdlibDupText(func->internalName);
            if ( callee->text == NULL ) {
                return 0;
            }
        }
    }

    /* 标准库命名空间：math.log -> __xl_std_math_log。 */
    if ( node->kind == XL6_AST_MEMBER && node->childCount > 0 && node->children[0] != NULL ) {
        rootName = xl6StdlibReceiverRootName(node->children[0]);
        if ( !xl6StdlibScopeContains(scope, rootName) &&
             xl6StdlibReceiverPath(node->children[0], ns, sizeof(ns)) ) {
            const XL6_StdlibConst* constant = xl6StdlibFindConst(ns, node->text);
            if ( constant != NULL ) {
                xl6StdlibRewriteMemberToLiteral(node, constant->kind, constant->value);
                return node->text != NULL;
            }
            const XL6_StdlibFunc* func = xl6StdlibFindFunc(ns, node->text, &index);
            if ( func != NULL ) {
                xl6StdlibMarkFuncUsed(used, ns, node->text);
                xl6StdlibRewriteMemberToName(node, func->internalName);
                return node->text != NULL;
            }
            if ( xl6StdlibNamespaceExists(ns) ) {
                if ( diags != NULL ) {
                    xl6DiagAdd(diags, node->line, node->column, "standard library namespace has no such symbol");
                }
                return 0;
            }
        }
    }

    ok = 1;
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6StdlibRewriteRefsScoped(node->children[i], used, diags, scope) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6StdlibNodeUsesTypeName(const XL6_AstNode* node, const char* typeName)
{
    size_t i;

    if ( node == NULL || typeName == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE && xl6StdlibTextEquals(node->text, typeName) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6StdlibTextEquals(node->children[0]->text, typeName) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6StdlibNodeUsesTypeName(node->children[i], typeName) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6StdlibNeedsRandomClass(const XL6_AstNode* root, const int* used)
{
    (void)used;
    return xl6StdlibNodeUsesTypeName(root, "Random");
}

static int xl6StdlibNeedsBufferClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibFindFunc("encoding", "toBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("encoding", "convertBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("file", "readBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("file", "writeBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("file", "appendBytes", &index) != NULL && used[index] ) {
        return 1;
    }
    return xl6StdlibNodeUsesTypeName(root, "Buffer");
}

static int xl6StdlibNeedsNetClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "TcpStream") ||
         xl6StdlibNodeUsesTypeName(root, "TcpServer") ||
         xl6StdlibNodeUsesTypeName(root, "UdpSocket") ||
         xl6StdlibNodeUsesTypeName(root, "UdpPacket") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("tcp", "connect", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("tcp", "connectAsync", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("tcp", "listen", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("udp", "bind", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsFileInfoClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "FileInfo") ) {
        return 1;
    }
    return xl6StdlibFindFunc("file", "info", &index) != NULL && used[index];
}

static int xl6StdlibNeedsFileClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "File") ) {
        return 1;
    }
    return xl6StdlibFindFunc("file", "open", &index) != NULL && used[index];
}

static int xl6StdlibNeedsRegexClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Regex") ||
         xl6StdlibNodeUsesTypeName(root, "RegexMatch") ||
         xl6StdlibNodeUsesTypeName(root, "RegexSet") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("regex", "compile", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("regex", "match", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("regex", "set", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsProcessClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    (void)root;
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("process", "run", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "runFile", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "cmd", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "shell", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "pipeline", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "start", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("process", "startFile", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsTccClass(const XL6_AstNode* root, const int* used)
{
    static const char* names[] = {
        "Context",
        "run",
        "runFile",
        "compile",
        "compileFile",
        "vfsMount",
        "vfsClear"
    };
    size_t i;
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "TccContext") ||
         xl6StdlibNodeUsesTypeName(root, "TccHandler") ) {
        return 1;
    }
    for ( i = 0; i < XL6_ARRAY_COUNT(names); ++i ) {
        if ( xl6StdlibFindFunc("tcc", names[i], &index) != NULL && used[index] ) {
            return 1;
        }
    }
    return 0;
}

static int xl6StdlibNeedsXlangClass(const XL6_AstNode* root, const int* used)
{
    static const char* names[] = {
        "Context",
        "run",
        "runFile"
    };
    size_t i;
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "XLangContext") ||
         xl6StdlibNodeUsesTypeName(root, "XLangHandler") ) {
        return 1;
    }
    for ( i = 0; i < XL6_ARRAY_COUNT(names); ++i ) {
        if ( xl6StdlibFindFunc("xlang", names[i], &index) != NULL && used[index] ) {
            return 1;
        }
    }
    return 0;
}

static int xl6StdlibNeedsLoggerClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Logger") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("log", "create", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("log", "default", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsTemplateClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Template") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("template", "compile", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("template", "render", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("template", "renderFile", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsCryptoClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "Sha256") ||
         xl6StdlibNodeUsesTypeName(root, "Sha384") ||
         xl6StdlibNodeUsesTypeName(root, "Sha512") ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("crypto", "sha256Stream", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("crypto", "sha384Stream", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("crypto", "sha512Stream", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsHttpServerClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "HttpRequest") ||
         xl6StdlibNodeUsesTypeName(root, "HttpResponse") ) {
        return 1;
    }
    if ( used == NULL ) {
        return 0;
    }
    if ( xl6StdlibFindFunc("http.server", "getRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "postRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "anyRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "routeStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "postStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "anyStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "appRouteStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "appPostStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "appAnyStreamRequest", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "error", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http.server", "appError", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static int xl6StdlibNeedsHttpClientClass(const XL6_AstNode* root, const int* used)
{
    size_t index;

    if ( xl6StdlibNodeUsesTypeName(root, "HttpClientRequest") ||
         xl6StdlibNodeUsesTypeName(root, "HttpClientResponse") ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http", "request", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http", "get", &index) != NULL && used[index] ) {
        return 1;
    }
    if ( xl6StdlibFindFunc("http", "post", &index) != NULL && used[index] ) {
        return 1;
    }
    return 0;
}

static void xl6StdlibAppendFuncDecl(XL6_StdlibTextBuilder* builder, const XL6_StdlibFunc* func)
{
    size_t i;

    if ( builder == NULL || func == NULL ) {
        return;
    }

    if ( func->sourceBody != NULL ) {
        xl6StdlibBuilderAppend(builder, func->sourceBody);
        return;
    }

    xl6StdlibBuilderAppend(builder, "extern ");
    xl6StdlibBuilderAppendQuoted(builder, func->cSymbol);
    xl6StdlibBuilderAppend(builder, " func ");
    xl6StdlibBuilderAppend(builder, func->internalName);
    xl6StdlibBuilderAppend(builder, "(");
    for ( i = 0; i < func->paramCount; ++i ) {
        if ( i > 0 ) {
            xl6StdlibBuilderAppend(builder, ", ");
        }
        xl6StdlibBuilderAppend(builder, func->params[i].type);
        xl6StdlibBuilderAppend(builder, " ");
        xl6StdlibBuilderAppend(builder, func->params[i].name);
    }
    xl6StdlibBuilderAppend(builder, ")");
    if ( func->returnType != NULL && func->returnType[0] != '\0' ) {
        xl6StdlibBuilderAppend(builder, " as ");
        xl6StdlibBuilderAppend(builder, func->returnType);
    }
    xl6StdlibBuilderAppend(builder, "\n");
}

static int xl6StdlibMergeGeneratedDecls(XL6_AstNode* root, const char* generated, XL6_DiagList* diags)
{
    XL6_ParseResult result;
    int ok;

    if ( generated == NULL || generated[0] == '\0' ) {
        return 1;
    }

    xl6ParseResultInit(&result);
    ok = xl6ParseSource(generated, "<stdlib>", &result);
    if ( !ok ) {
        if ( diags != NULL && result.diags.count > 0 ) {
            size_t i;
            for ( i = 0; i < result.diags.count; ++i ) {
                xl6DiagAdd(diags, result.diags.items[i].line, result.diags.items[i].column, result.diags.items[i].message);
            }
        } else if ( diags != NULL ) {
            xl6DiagAdd(diags, 1, 1, "standard library generated invalid extern declarations");
        }
        xl6ParseResultFree(&result);
        return 0;
    }
    if ( result.root != NULL ) {
        xl6AstMoveChildren(root, result.root);
    }
    xl6ParseResultFree(&result);
    return 1;
}

int xl6StdlibExpandModule(XL6_AstNode* root, XL6_DiagList* diags)
{
    int used[XL6_STDLIB_FUNC_COUNT];
    XL6_StdlibTextBuilder builder;
    size_t i;
    int needsRandomClass;
    int needsBufferClass;
    int needsNetClass;
    int needsFileInfoClass;
    int needsFileClass;
    int needsRegexClass;
    int needsProcessClass;
    int needsLoggerClass;
    int needsTemplateClass;
    int needsCryptoClass;
    int needsHttpServerClass;
    int needsHttpClientClass;
    int needsTccClass;
    int needsXlangClass;
    int ok;

    if ( root == NULL ) {
        return 1;
    }

    memset(used, 0, sizeof(used));
    ok = xl6StdlibRewriteRefsScoped(root, used, diags, NULL);
    if ( !ok ) {
        return 0;
    }
    needsRandomClass = xl6StdlibNeedsRandomClass(root, used);
    needsNetClass = xl6StdlibNeedsNetClass(root, used);
    needsHttpServerClass = xl6StdlibNeedsHttpServerClass(root, used);
    needsHttpClientClass = xl6StdlibNeedsHttpClientClass(root, used);
    needsFileInfoClass = xl6StdlibNeedsFileInfoClass(root, used);
    needsFileClass = xl6StdlibNeedsFileClass(root, used);
    needsBufferClass = xl6StdlibNeedsBufferClass(root, used) || needsNetClass || needsHttpServerClass || needsHttpClientClass || needsFileClass;
    needsRegexClass = xl6StdlibNeedsRegexClass(root, used);
    needsProcessClass = xl6StdlibNeedsProcessClass(root, used);
    needsLoggerClass = xl6StdlibNeedsLoggerClass(root, used);
    needsTemplateClass = xl6StdlibNeedsTemplateClass(root, used);
    needsCryptoClass = xl6StdlibNeedsCryptoClass(root, used);
    needsTccClass = xl6StdlibNeedsTccClass(root, used);
    needsXlangClass = xl6StdlibNeedsXlangClass(root, used);
    if ( needsRegexClass ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
    }
    if ( needsCryptoClass ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
    }
    if ( needsFileClass ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
    }
    if ( needsHttpClientClass ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
    }
    if ( needsRandomClass || needsBufferClass || needsNetClass || needsFileClass || needsRegexClass || needsProcessClass || needsHttpServerClass || needsHttpClientClass || needsTemplateClass || needsCryptoClass || needsXlangClass ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
    }
    xl6StdlibApplyDeps(used);

    /* 只为实际使用的标准库函数注入 extern 声明。 */
    xl6StdlibBuilderInit(&builder);
    xl6StdlibBuilderAppend(&builder, xl6StdlibCoreClassSource);
    if ( needsRandomClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibRandomClassSource);
    }
    if ( needsBufferClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibBufferClassSource);
    }
    if ( needsLoggerClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibLoggerClassSource);
    }
    if ( needsTemplateClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTemplateClassSource);
    }
    if ( needsCryptoClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibCryptoClassSource);
    }
    if ( needsHttpServerClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibHttpServerClassSource);
    }
    if ( needsHttpClientClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibHttpClientClassSource);
    }
    if ( needsNetClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibNetClassSource);
    }
    if ( needsFileInfoClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibFileInfoClassSource);
    }
    if ( needsFileClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibFileClassSource);
    }
    if ( needsRegexClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibRegexClassSource);
    }
    if ( needsProcessClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibProcessClassSource);
    }
    if ( needsTccClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibTccClassSource);
    }
    if ( needsXlangClass ) {
        xl6StdlibBuilderAppend(&builder, xl6StdlibXlangClassSource);
    }
    for ( i = 0; i < XL6_STDLIB_FUNC_COUNT; ++i ) {
        if ( used[i] ) {
            xl6StdlibAppendFuncDecl(&builder, &xl6StdlibFuncs[i]);
        }
    }
    ok = builder.ok && xl6StdlibMergeGeneratedDecls(root, builder.data, diags);
    xl6StdlibBuilderFree(&builder);
    return ok;
}
