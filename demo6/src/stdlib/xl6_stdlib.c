#include "src/stdlib/xl6_stdlib.h"

#include "src/common/xl6_common.h"
#include "src/parser/xl6_parser_api.h"

#include <stdio.h>

typedef struct XL6_StdlibTextBuilder {
    char* data;
    size_t len;
    size_t capacity;
    int ok;
} XL6_StdlibTextBuilder;

typedef struct XL6_StdlibParam {
    const char* type;
    const char* name;
} XL6_StdlibParam;

typedef struct XL6_StdlibFunc {
    const char* ns;
    const char* name;
    const char* internalName;
    const char* cSymbol;
    const XL6_StdlibParam* params;
    size_t paramCount;
    const char* returnType;
    const char* sourceBody;
} XL6_StdlibFunc;

static const XL6_StdlibParam xl6StdMathUnaryFloatParams[] = {
    {"float", "x"}
};

static const XL6_StdlibParam xl6StdMathPowParams[] = {
    {"float", "x"},
    {"float", "y"}
};

static const XL6_StdlibParam xl6StdMathAtan2Params[] = {
    {"float", "y"},
    {"float", "x"}
};

static const XL6_StdlibParam xl6StdStringLenParams[] = {
    {"string", "text"}
};

static const XL6_StdlibParam xl6StdXrtStrCompParams[] = {
    {"string", "left"},
    {"string", "right"},
    {"c_size", "size"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtInStrParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "part"},
    {"c_size", "partSize"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtCheckStrParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "chars"},
    {"c_size", "charsSize"}
};

static const XL6_StdlibParam xl6StdXrtStrLikeParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "pattern"},
    {"c_size", "patternSize"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdXrtStrApproxParams[] = {
    {"string", "left"},
    {"c_size", "leftSize"},
    {"string", "right"},
    {"c_size", "rightSize"}
};

static const XL6_StdlibParam xl6StdXrtIsUtf8Params[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtIntFormatParams[] = {
    {"int", "value"},
    {"string", "format"}
};

static const XL6_StdlibParam xl6StdXrtNumFormatParams[] = {
    {"float", "value"},
    {"string", "format"}
};

static const XL6_StdlibParam xl6StdRuntimeTakeCstrParams[] = {
    {"point", "value"}
};

static const XL6_StdlibParam xl6StdRuntimePrintParams[] = {
    {"var", "values"},
    {"bool", "newline"}
};

static const XL6_StdlibParam xl6StdRuntimeStringSplitParams[] = {
    {"string", "text"},
    {"string", "sep"}
};

static const XL6_StdlibParam xl6StdRuntimeStringEdgeParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "edge"},
    {"c_size", "edgeSize"},
    {"c_bool", "ignoreCase"}
};

static const XL6_StdlibParam xl6StdRuntimeCryptoTextParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeUrlPartParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_int", "part"}
};

static const XL6_StdlibParam xl6StdRuntimeUrlTextParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeUrlResolveParams[] = {
    {"string", "base"},
    {"c_size", "baseSize"},
    {"string", "ref"},
    {"c_size", "refSize"}
};

static const XL6_StdlibParam xl6StdRuntimeQueryTextParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdRuntimeQueryPairParams[] = {
    {"string", "name"},
    {"c_size", "nameSize"},
    {"string", "value"},
    {"c_size", "valueSize"}
};

static const XL6_StdlibParam xl6StdRuntimeQueryAppendParams[] = {
    {"string", "query"},
    {"c_size", "querySize"},
    {"string", "name"},
    {"c_size", "nameSize"},
    {"string", "value"},
    {"c_size", "valueSize"}
};

static const XL6_StdlibParam xl6StdRuntimeRegexTextParams[] = {
    {"string", "text"},
    {"c_size", "textSize"},
    {"string", "pattern"}
};

static const XL6_StdlibParam xl6StdRuntimeProcessExecParams[] = {
    {"string", "command"},
    {"c_int", "timeoutMs"}
};

static const XL6_StdlibParam xl6StdRuntimeDirListParams[] = {
    {"string", "path"},
    {"c_bool", "recursive"},
    {"c_int", "mode"}
};

static const XL6_StdlibParam xl6StdRuntimePathJoinParams[] = {
    {"string", "first"},
    {"string", "second"},
    {"string", "third"},
    {"string", "fourth"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerCreateParams[] = {
    {"c_int", "port"},
    {"func(string, string, string, string) as string", "handler"}
};

static const XL6_StdlibParam xl6StdRuntimeHttpServerHandleParams[] = {
    {"point", "server"}
};

static const XL6_StdlibParam xl6StdXrtStringCaseParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_bool", "revise"}
};

static const XL6_StdlibParam xl6StdXrtStringTrimParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"string", "chars"},
    {"c_size", "charsSize"},
    {"c_bool", "revise"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtStringReplaceParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"string", "oldText"},
    {"c_size", "oldSize"},
    {"string", "newText"},
    {"c_size", "newSize"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtTimeToStrParams[] = {
    {"int", "value"},
    {"c_int", "format"}
};

static const XL6_StdlibParam xl6StdXrtTimeFormatParams[] = {
    {"int", "value"},
    {"string", "pattern"}
};

static const XL6_StdlibParam xl6StdXrtTimeParseParams[] = {
    {"string", "text"},
    {"string", "pattern"}
};

static const XL6_StdlibParam xl6StdXrtYearParams[] = {
    {"c_int", "year"}
};

static const XL6_StdlibParam xl6StdXrtYearMonthParams[] = {
    {"c_int", "year"},
    {"c_int", "month"}
};

static const XL6_StdlibParam xl6StdTimeFromTimeParams[] = {
    {"c_int", "hour"},
    {"c_int", "minute"},
    {"c_int", "second"}
};

static const XL6_StdlibParam xl6StdTimeFromDateParams[] = {
    {"int", "year"},
    {"c_int", "month"},
    {"c_int", "day"}
};

static const XL6_StdlibParam xl6StdTimeFromDateTimeParams[] = {
    {"int", "year"},
    {"c_int", "month"},
    {"c_int", "day"},
    {"c_int", "hour"},
    {"c_int", "minute"},
    {"c_int", "second"}
};

static const XL6_StdlibParam xl6StdTimeUnaryParams[] = {
    {"time", "value"}
};

static const XL6_StdlibParam xl6StdTimeBinaryParams[] = {
    {"time", "left"},
    {"time", "right"}
};

static const XL6_StdlibParam xl6StdTimeInRangeParams[] = {
    {"time", "value"},
    {"time", "start"},
    {"time", "endValue"}
};

static const XL6_StdlibParam xl6StdXrtDateAddParams[] = {
    {"c_int", "interval"},
    {"int", "amount"},
    {"int", "value"}
};

static const XL6_StdlibParam xl6StdXrtDateDiffParams[] = {
    {"c_int", "interval"},
    {"int", "left"},
    {"int", "right"}
};

static const XL6_StdlibParam xl6StdXrtStrToTimeParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtIntUnaryParams[] = {
    {"int", "value"}
};

static const XL6_StdlibParam xl6StdXrtIntBinaryParams[] = {
    {"int", "left"},
    {"int", "right"}
};

static const XL6_StdlibParam xl6StdXrtIntInRangeParams[] = {
    {"int", "value"},
    {"int", "start"},
    {"int", "endValue"}
};

static const XL6_StdlibParam xl6StdXrtTimeOverlapParams[] = {
    {"int", "start1"},
    {"int", "end1"},
    {"int", "start2"},
    {"int", "end2"}
};

static const XL6_StdlibParam xl6StdTimeSleepParams[] = {
    {"int", "ms"}
};

static const XL6_StdlibParam xl6StdXrtSleepParams[] = {
    {"c_uint", "ms"}
};

static const XL6_StdlibParam xl6StdXrtWeekBoundaryParams[] = {
    {"int", "value"},
    {"c_int", "startDay"}
};

static const XL6_StdlibParam xl6StdXrtRandRangeParams[] = {
    {"c_int", "min"},
    {"c_int", "max"}
};

static const XL6_StdlibParam xl6StdXrtRandStrParams[] = {
    {"string", "chars"},
    {"c_size", "size"},
    {"c_size", "len"}
};

static const XL6_StdlibParam xl6StdPathUnaryParams[] = {
    {"string", "path"}
};

static const XL6_StdlibParam xl6StdXrtPathIsAbsParams[] = {
    {"string", "path"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtPathRandomParams[] = {
    {"string", "head"},
    {"c_size", "headSize"},
    {"string", "foot"},
    {"c_size", "footSize"},
    {"c_size", "len"}
};

static const XL6_StdlibParam xl6StdFileWriteTextParams[] = {
    {"string", "path"},
    {"string", "text"},
    {"int", "charset"}
};

static const XL6_StdlibParam xl6StdFileSetSizeParams[] = {
    {"string", "path"},
    {"int", "size"}
};

static const XL6_StdlibParam xl6StdXrtFileSetSizeParams[] = {
    {"string", "path"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdFileSetAttrParams[] = {
    {"string", "path"},
    {"int", "attr"}
};

static const XL6_StdlibParam xl6StdXrtFileSetAttrParams[] = {
    {"string", "path"},
    {"c_int", "attr"}
};

static const XL6_StdlibParam xl6StdXrtFileWriteTextParams[] = {
    {"string", "path"},
    {"string", "text"},
    {"c_size", "size"},
    {"c_int", "charset"}
};

static const XL6_StdlibParam xl6StdFileReadTextParams[] = {
    {"string", "path"},
    {"int", "charset"}
};

static const XL6_StdlibParam xl6StdXrtFileReadTextParams[] = {
    {"string", "path"},
    {"c_int", "charset"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdFileCopyMoveParams[] = {
    {"string", "source"},
    {"string", "target"},
    {"bool", "overwrite"}
};

static const XL6_StdlibParam xl6StdXrtFileCopyMoveParams[] = {
    {"string", "source"},
    {"string", "target"},
    {"c_bool", "overwrite"}
};

static const XL6_StdlibParam xl6StdXrtJsonParseParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtJsonStringifyParams[] = {
    {"var", "value"},
    {"c_int", "format"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtXsonStringifyParams[] = {
    {"var", "value"},
    {"c_int", "format"},
    {"c_uint", "flags"},
    {"point", "retSize"}
};

static const XL6_StdlibParam xl6StdXrtJsonWriteParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"c_int", "format"}
};

static const XL6_StdlibParam xl6StdXrtXsonWriteParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"c_int", "format"},
    {"c_uint", "flags"}
};

static const XL6_StdlibParam xl6StdJsonStringifyParams[] = {
    {"var", "value"},
    {"bool", "pretty"}
};

static const XL6_StdlibParam xl6StdJsonWriteParams[] = {
    {"string", "path"},
    {"var", "value"},
    {"bool", "pretty"}
};

static const XL6_StdlibParam xl6StdXrtCodecTextParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtCodecBase64Params[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"point", "table"}
};

static const XL6_StdlibParam xl6StdXrtHashParams[] = {
    {"string", "text"},
    {"c_size", "size"}
};

static const XL6_StdlibParam xl6StdXrtHash32SeedParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_uint", "seed"}
};

static const XL6_StdlibParam xl6StdXrtHash64SeedParams[] = {
    {"string", "text"},
    {"c_size", "size"},
    {"c_ulonglong", "seed"}
};

static const char xl6StdMathRoundBody[] =
    "func __xl_std_math_round(float x) as float\n"
    "    if x >= 0.0 then\n"
    "        return __xl_std_math_floor(x + 0.5)\n"
    "    else\n"
    "        return __xl_std_math_ceil(x - 0.5)\n"
    "    end\n"
    "end\n";

static const char xl6StdStringCompareBody[] =
    "func __xl_std_string_compare(string left, string right, bool ignoreCase = false) as int\n"
    "    return __xl_std_xrt_str_comp(left, right, 0, ignoreCase)\n"
    "end\n";

static const char xl6StdStringLenBody[] =
    "func __xl_std_string_len(string text) as int\n"
    "    return int(__xl_std_crt_strlen(text))\n"
    "end\n";

static const char xl6StdStringEqualsBody[] =
    "func __xl_std_string_equals(string left, string right, bool ignoreCase = false) as bool\n"
    "    return __xl_std_xrt_str_comp(left, right, 0, ignoreCase) == 0\n"
    "end\n";

static const char xl6StdStringContainsBody[] =
    "func __xl_std_string_contains(string text, string part, bool ignoreCase = false) as bool\n"
    "    return __xl_std_xrt_in_str(text, 0, part, 0, ignoreCase) != 0\n"
    "end\n";

static const char xl6StdStringIndexOfBody[] =
    "func __xl_std_string_index_of(string text, string part, bool ignoreCase = false) as int\n"
    "    int pos = __xl_std_xrt_in_str(text, 0, part, 0, ignoreCase)\n"
    "    if pos == 0 then\n"
    "        return -1\n"
    "    end\n"
    "    return pos - 1\n"
    "end\n";

static const char xl6StdStringStartsWithBody[] =
    "func __xl_std_string_starts_with(string text, string prefix, bool ignoreCase = false) as bool\n"
    "    return __xl_std_runtime_string_starts_with(text, __xl_std_crt_strlen(text), prefix, __xl_std_crt_strlen(prefix), ignoreCase)\n"
    "end\n";

static const char xl6StdStringEndsWithBody[] =
    "func __xl_std_string_ends_with(string text, string suffix, bool ignoreCase = false) as bool\n"
    "    return __xl_std_runtime_string_ends_with(text, __xl_std_crt_strlen(text), suffix, __xl_std_crt_strlen(suffix), ignoreCase)\n"
    "end\n";

static const char xl6StdStringHasAnyBody[] =
    "func __xl_std_string_has_any(string text, string chars) as bool\n"
    "    return __xl_std_xrt_check_str(text, 0, chars, 0) != null\n"
    "end\n";

static const char xl6StdStringLikeBody[] =
    "func __xl_std_string_like(string text, string pattern, bool ignoreCase = false) as bool\n"
    "    return __xl_std_xrt_str_like(text, 0, pattern, 0, ignoreCase)\n"
    "end\n";

static const char xl6StdStringApproxBody[] =
    "func __xl_std_string_approx(string left, string right) as bool\n"
    "    return __xl_std_xrt_str_approx(left, 0, right, 0)\n"
    "end\n";

static const char xl6StdStringSimBody[] =
    "func __xl_std_string_sim(string left, string right) as float\n"
    "    return float(__xl_std_xrt_str_sim(left, 0, right, 0))\n"
    "end\n";

static const char xl6StdStringIsUtf8Body[] =
    "func __xl_std_string_is_utf8(string text) as bool\n"
    "    return __xl_std_xrt_is_utf8(text, 0)\n"
    "end\n";

static const char xl6StdStringFormatIntBody[] =
    "func __xl_std_string_format_int(int value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_int_format(value, format))\n"
    "end\n";

static const char xl6StdStringFormatFloatBody[] =
    "func __xl_std_string_format_float(float value, string format = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_num_format(value, format))\n"
    "end\n";

static const char xl6StdStringLowerBody[] =
    "func __xl_std_string_lower(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_lcase(text, 0, false))\n"
    "end\n";

static const char xl6StdStringUpperBody[] =
    "func __xl_std_string_upper(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_ucase(text, 0, false))\n"
    "end\n";

static const char xl6StdStringTrimBody[] =
    "func __xl_std_string_trim(string text, string chars = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_trim(text, 0, chars, 0, false, null))\n"
    "end\n";

static const char xl6StdStringLTrimBody[] =
    "func __xl_std_string_ltrim(string text, string chars = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_ltrim(text, 0, chars, 0, false, null))\n"
    "end\n";

static const char xl6StdStringRTrimBody[] =
    "func __xl_std_string_rtrim(string text, string chars = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_rtrim(text, 0, chars, 0, false, null))\n"
    "end\n";

static const char xl6StdStringFilterBody[] =
    "func __xl_std_string_filter(string text, string chars) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_filter(text, 0, chars, 0, false, null))\n"
    "end\n";

static const char xl6StdStringReplaceBody[] =
    "func __xl_std_string_replace(string text, string oldText, string newText) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_replace(text, 0, oldText, 0, newText, 0, null))\n"
    "end\n";

static const char xl6StdStringSplitBody[] =
    "func __xl_std_string_split(string text, string sep) as array\n"
    "    return array(__xl_std_runtime_string_split(text, sep))\n"
    "end\n";

static const char xl6StdRegexIsMatchBody[] =
    "func __xl_std_regex_is_match(string text, string pattern) as bool\n"
    "    return __xl_std_runtime_regex_is_match(text, __xl_std_crt_strlen(text), pattern)\n"
    "end\n";

static const char xl6StdRegexFindBody[] =
    "func __xl_std_regex_find(string text, string pattern) as int\n"
    "    return int(__xl_std_runtime_regex_find(text, __xl_std_crt_strlen(text), pattern))\n"
    "end\n";

static const char xl6StdProcessExecBody[] =
    "func __xl_std_process_exec(string command, int timeoutMs = 0) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_process_exec(command, c_int(timeoutMs)))\n"
    "end\n";

static const char xl6StdProcessSuccessBody[] =
    "func __xl_std_process_success(string command, int timeoutMs = 0) as bool\n"
    "    return __xl_std_runtime_process_success(command, c_int(timeoutMs))\n"
    "end\n";

static const char xl6StdProcessExitCodeBody[] =
    "func __xl_std_process_exit_code(string command, int timeoutMs = 0) as int\n"
    "    return int(__xl_std_runtime_process_exit_code(command, c_int(timeoutMs)))\n"
    "end\n";

static const char xl6StdTimeFromDateTimeBody[] =
    "func __xl_std_time_from_datetime(int year, int month, int day, int hour = 0, int minute = 0, int second = 0) as time\n"
    "    return time(__xl_std_xrt_datetime_serial(year, month, day, hour, minute, second))\n"
    "end\n";

static const char xl6StdTimeNowBody[] =
    "func __xl_std_time_now() as time\n"
    "    return time(__xl_std_xrt_now())\n"
    "end\n";

static const char xl6StdTimeTodayBody[] =
    "func __xl_std_time_today() as time\n"
    "    return time(__xl_std_xrt_date())\n"
    "end\n";

static const char xl6StdTimeClockBody[] =
    "func __xl_std_time_clock() as time\n"
    "    return time(__xl_std_xrt_time())\n"
    "end\n";

static const char xl6StdTimeFromTimeBody[] =
    "func __xl_std_time_from_time(int hour, int minute, int second) as time\n"
    "    return time(__xl_std_xrt_time_serial(hour, minute, second))\n"
    "end\n";

static const char xl6StdTimeFromDateBody[] =
    "func __xl_std_time_from_date(int year, int month, int day) as time\n"
    "    return time(__xl_std_xrt_date_serial(year, month, day))\n"
    "end\n";

static const char xl6StdTimeParseBody[] =
    "func __xl_std_time_parse(string text) as time\n"
    "    return time(__xl_std_xrt_str_to_time(text, 0))\n"
    "end\n";

static const char xl6StdTimeParsePatternBody[] =
    "func __xl_std_time_parse_pattern(string text, string pattern) as time\n"
    "    return time(__xl_std_xrt_time_parse(text, pattern))\n"
    "end\n";

static const char xl6StdTimeYearBody[] =
    "func __xl_std_time_year(time value) as int\n"
    "    return __xl_std_xrt_year(int(value))\n"
    "end\n";

static const char xl6StdTimeMonthBody[] =
    "func __xl_std_time_month(time value) as int\n"
    "    return __xl_std_xrt_month(int(value))\n"
    "end\n";

static const char xl6StdTimeDayBody[] =
    "func __xl_std_time_day(time value) as int\n"
    "    return __xl_std_xrt_day(int(value))\n"
    "end\n";

static const char xl6StdTimeHourBody[] =
    "func __xl_std_time_hour(time value) as int\n"
    "    return __xl_std_xrt_hour(int(value))\n"
    "end\n";

static const char xl6StdTimeMinuteBody[] =
    "func __xl_std_time_minute(time value) as int\n"
    "    return __xl_std_xrt_minute(int(value))\n"
    "end\n";

static const char xl6StdTimeSecondBody[] =
    "func __xl_std_time_second(time value) as int\n"
    "    return __xl_std_xrt_second(int(value))\n"
    "end\n";

static const char xl6StdTimeWeekdayBody[] =
    "func __xl_std_time_weekday(time value) as int\n"
    "    return __xl_std_xrt_weekday(int(value))\n"
    "end\n";

static const char xl6StdTimeDayOfYearBody[] =
    "func __xl_std_time_day_of_year(time value) as int\n"
    "    return __xl_std_xrt_day_of_year(int(value))\n"
    "end\n";

static const char xl6StdTimeQuarterBody[] =
    "func __xl_std_time_quarter(time value) as int\n"
    "    return __xl_std_xrt_quarter(int(value))\n"
    "end\n";

static const char xl6StdTimeIsLeapYearBody[] =
    "func __xl_std_time_is_leap_year(int year) as bool\n"
    "    return __xl_std_xrt_is_leap_year(c_int(year))\n"
    "end\n";

static const char xl6StdTimeDaysInMonthBody[] =
    "func __xl_std_time_days_in_month(int year, int month) as int\n"
    "    return int(__xl_std_xrt_days_in_month(c_int(year), c_int(month)))\n"
    "end\n";

static const char xl6StdTimeDaysInYearBody[] =
    "func __xl_std_time_days_in_year(int year) as int\n"
    "    return int(__xl_std_xrt_days_in_year(c_int(year)))\n"
    "end\n";

static const char xl6StdTimeDatePartBody[] =
    "func __xl_std_time_date_part(time value) as time\n"
    "    return time(__xl_std_xrt_date_part(int(value)))\n"
    "end\n";

static const char xl6StdTimeTimePartBody[] =
    "func __xl_std_time_time_part(time value) as time\n"
    "    return time(__xl_std_xrt_time_part(int(value)))\n"
    "end\n";

static const char xl6StdTimeIsSameDayBody[] =
    "func __xl_std_time_is_same_day(time left, time right) as bool\n"
    "    return __xl_std_xrt_is_same_day(int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeIsSameMonthBody[] =
    "func __xl_std_time_is_same_month(time left, time right) as bool\n"
    "    return __xl_std_xrt_is_same_month(int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeIsSameYearBody[] =
    "func __xl_std_time_is_same_year(time left, time right) as bool\n"
    "    return __xl_std_xrt_is_same_year(int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeInRangeBody[] =
    "func __xl_std_time_in_range(time value, time start, time endValue) as bool\n"
    "    return __xl_std_xrt_time_in_range(int(value), int(start), int(endValue))\n"
    "end\n";

static const char xl6StdTimeAddYearsBody[] =
    "func __xl_std_time_add_years(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(1, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddMonthsBody[] =
    "func __xl_std_time_add_months(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(2, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddDaysBody[] =
    "func __xl_std_time_add_days(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(3, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddHoursBody[] =
    "func __xl_std_time_add_hours(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(4, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddMinutesBody[] =
    "func __xl_std_time_add_minutes(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(5, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeAddSecondsBody[] =
    "func __xl_std_time_add_seconds(time value, int amount) as time\n"
    "    return time(__xl_std_xrt_date_add(6, amount, int(value)))\n"
    "end\n";

static const char xl6StdTimeDiffDaysBody[] =
    "func __xl_std_time_diff_days(time left, time right) as int\n"
    "    return __xl_std_xrt_date_diff(3, int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeTimerBody[] =
    "func __xl_std_time_timer() as float\n"
    "    return __xl_std_xrt_timer()\n"
    "end\n";

static const char xl6StdTimeSleepBody[] =
    "func __xl_std_time_sleep(int ms)\n"
    "    __xl_std_xrt_sleep(ms)\n"
    "end\n";

static const char xl6StdTimeNowUtcBody[] =
    "func __xl_std_time_now_utc() as time\n"
    "    return time(__xl_std_xrt_now_utc())\n"
    "end\n";

static const char xl6StdTimeTimezoneOffsetBody[] =
    "func __xl_std_time_timezone_offset() as int\n"
    "    return __xl_std_xrt_timezone_offset()\n"
    "end\n";

static const char xl6StdTimeRangeOverlapBody[] =
    "func __xl_std_time_range_overlap(time start1, time end1, time start2, time end2) as bool\n"
    "    return __xl_std_xrt_time_range_overlap(int(start1), int(end1), int(start2), int(end2))\n"
    "end\n";

static const char xl6StdTimeApproxBody[] =
    "func __xl_std_time_approx(time left, time right) as bool\n"
    "    return __xl_std_xrt_time_approx(int(left), int(right))\n"
    "end\n";

static const char xl6StdTimeToUnixBody[] =
    "func __xl_std_time_to_unix(time value) as int\n"
    "    return __xl_std_xrt_time_to_unix(int(value))\n"
    "end\n";

static const char xl6StdTimeFromUnixBody[] =
    "func __xl_std_time_from_unix(int seconds) as time\n"
    "    return time(__xl_std_xrt_time_from_unix(seconds))\n"
    "end\n";

static const char xl6StdTimeUtcToLocalBody[] =
    "func __xl_std_time_utc_to_local(time value) as time\n"
    "    return time(__xl_std_xrt_utc_to_local(int(value)))\n"
    "end\n";

static const char xl6StdTimeLocalToUtcBody[] =
    "func __xl_std_time_local_to_utc(time value) as time\n"
    "    return time(__xl_std_xrt_local_to_utc(int(value)))\n"
    "end\n";

static const char xl6StdTimeFirstDayOfMonthBody[] =
    "func __xl_std_time_first_day_of_month(time value) as time\n"
    "    return time(__xl_std_xrt_first_day_of_month(int(value)))\n"
    "end\n";

static const char xl6StdTimeLastDayOfMonthBody[] =
    "func __xl_std_time_last_day_of_month(time value) as time\n"
    "    return time(__xl_std_xrt_last_day_of_month(int(value)))\n"
    "end\n";

static const char xl6StdTimeFirstDayOfYearBody[] =
    "func __xl_std_time_first_day_of_year(time value) as time\n"
    "    return time(__xl_std_xrt_first_day_of_year(int(value)))\n"
    "end\n";

static const char xl6StdTimeLastDayOfYearBody[] =
    "func __xl_std_time_last_day_of_year(time value) as time\n"
    "    return time(__xl_std_xrt_last_day_of_year(int(value)))\n"
    "end\n";

static const char xl6StdTimeFirstDayOfWeekBody[] =
    "func __xl_std_time_first_day_of_week(time value, int startDay = 1) as time\n"
    "    return time(__xl_std_xrt_first_day_of_week(int(value), startDay))\n"
    "end\n";

static const char xl6StdTimeLastDayOfWeekBody[] =
    "func __xl_std_time_last_day_of_week(time value, int startDay = 1) as time\n"
    "    return time(__xl_std_xrt_last_day_of_week(int(value), startDay))\n"
    "end\n";

static const char xl6StdTimeWeekOfYearBody[] =
    "func __xl_std_time_week_of_year(time value) as int\n"
    "    return __xl_std_xrt_week_of_year(int(value))\n"
    "end\n";

static const char xl6StdTimeWeekOfMonthBody[] =
    "func __xl_std_time_week_of_month(time value) as int\n"
    "    return __xl_std_xrt_week_of_month(int(value))\n"
    "end\n";

static const char xl6StdTimeFormatBody[] =
    "func __xl_std_time_format(time value, int format = 0) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_time_to_str(int(value), format))\n"
    "end\n";

static const char xl6StdTimeFormatPatternBody[] =
    "func __xl_std_time_format_pattern(time value, string pattern) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_time_format(int(value), pattern))\n"
    "end\n";

static const char xl6StdTimeDateStringBody[] =
    "func __xl_std_time_date_string(time value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_time_to_str(int(value), 1))\n"
    "end\n";

static const char xl6StdTimeTimeStringBody[] =
    "func __xl_std_time_time_string(time value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_time_to_str(int(value), 2))\n"
    "end\n";

static const char xl6StdRandomU32Body[] =
    "func __xl_std_random_u32() as uint32\n"
    "    return uint32(__xl_std_xrt_rand32())\n"
    "end\n";

static const char xl6StdRandomU64Body[] =
    "func __xl_std_random_u64() as uint64\n"
    "    return uint64(__xl_std_xrt_rand64())\n"
    "end\n";

static const char xl6StdRandomRangeBody[] =
    "func __xl_std_random_range(int min, int max) as int\n"
    "    return __xl_std_xrt_rand_range(min, max)\n"
    "end\n";

static const char xl6StdRandomStringBody[] =
    "func __xl_std_random_string(int len = 16, string chars = \"\") as string\n"
    "    if len < 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_rand_str(chars, 0, c_size(len)))\n"
    "end\n";

static const char xl6StdPathExistsBody[] =
    "func __xl_std_path_exists(string path) as bool\n"
    "    return __xl_std_xrt_path_exists(path)\n"
    "end\n";

static const char xl6StdPathIsAbsBody[] =
    "func __xl_std_path_is_abs(string path) as bool\n"
    "    return __xl_std_xrt_path_is_abs(path, 0)\n"
    "end\n";

static const char xl6StdPathNameExtBody[] =
    "func __xl_std_path_name_ext(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_name_ext(path, 0))\n"
    "end\n";

static const char xl6StdPathNameBody[] =
    "func __xl_std_path_name(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_name(path, 0))\n"
    "end\n";

static const char xl6StdPathExtBody[] =
    "func __xl_std_path_ext(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_ext(path, 0))\n"
    "end\n";

static const char xl6StdPathDirBody[] =
    "func __xl_std_path_dir(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_get_dir(path, 0))\n"
    "end\n";

static const char xl6StdPathJoinBody[] =
    "func __xl_std_path_join(string first, string second, string third = \"\", string fourth = \"\") as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_path_join(first, second, third, fourth))\n"
    "end\n";

static const char xl6StdPathRandomBody[] =
    "func __xl_std_path_random(string head = \"\", string foot = \"\", int len = 12) as string\n"
    "    if len < 0 then\n"
    "        return \"\"\n"
    "    end\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_path_random(head, 0, foot, 0, c_size(len)))\n"
    "end\n";

static const char xl6StdFileExistsBody[] =
    "func __xl_std_file_exists(string path) as bool\n"
    "    return __xl_std_xrt_file_exists(path)\n"
    "end\n";

static const char xl6StdFileSizeBody[] =
    "func __xl_std_file_size(string path) as int\n"
    "    return int(__xl_std_xrt_file_get_size(path))\n"
    "end\n";

static const char xl6StdFileSetSizeBody[] =
    "func __xl_std_file_set_size(string path, int size) as bool\n"
    "    return __xl_std_xrt_file_set_size(path, size)\n"
    "end\n";

static const char xl6StdFileAttrBody[] =
    "func __xl_std_file_attr(string path) as int\n"
    "    return __xl_std_xrt_file_get_attr(path)\n"
    "end\n";

static const char xl6StdFileSetAttrBody[] =
    "func __xl_std_file_set_attr(string path, int attr) as bool\n"
    "    return __xl_std_xrt_file_set_attr(path, attr)\n"
    "end\n";

static const char xl6StdFileAccessTimeBody[] =
    "func __xl_std_file_access_time(string path) as time\n"
    "    return time(__xl_std_xrt_file_get_access_time(path))\n"
    "end\n";

static const char xl6StdFileChangeTimeBody[] =
    "func __xl_std_file_change_time(string path) as time\n"
    "    return time(__xl_std_xrt_file_get_change_time(path))\n"
    "end\n";

static const char xl6StdFileWriteTextBody[] =
    "func __xl_std_file_write_text(string path, string text, int charset = 65001) as bool\n"
    "    return __xl_std_xrt_file_write_all(path, text, 0, charset) != 0\n"
    "end\n";

static const char xl6StdFileAppendTextBody[] =
    "func __xl_std_file_append_text(string path, string text, int charset = 65001) as bool\n"
    "    return __xl_std_xrt_file_append(path, text, 0, charset) != 0\n"
    "end\n";

static const char xl6StdFileReadTextBody[] =
    "func __xl_std_file_read_text(string path, int charset = 65001) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_file_read_all(path, charset, null))\n"
    "end\n";

static const char xl6StdFileCopyBody[] =
    "func __xl_std_file_copy(string source, string target, bool overwrite = true) as bool\n"
    "    return __xl_std_xrt_file_copy(source, target, overwrite)\n"
    "end\n";

static const char xl6StdFileMoveBody[] =
    "func __xl_std_file_move(string source, string target, bool overwrite = true) as bool\n"
    "    return __xl_std_xrt_file_move(source, target, overwrite)\n"
    "end\n";

static const char xl6StdFileDeleteBody[] =
    "func __xl_std_file_delete(string path) as bool\n"
    "    return __xl_std_xrt_file_delete(path)\n"
    "end\n";

static const char xl6StdDirExistsBody[] =
    "func __xl_std_dir_exists(string path) as bool\n"
    "    return __xl_std_xrt_dir_exists(path)\n"
    "end\n";

static const char xl6StdDirCreateBody[] =
    "func __xl_std_dir_create(string path) as bool\n"
    "    return __xl_std_xrt_dir_create(path)\n"
    "end\n";

static const char xl6StdDirCreateAllBody[] =
    "func __xl_std_dir_create_all(string path) as bool\n"
    "    return __xl_std_xrt_dir_create_all(path)\n"
    "end\n";

static const char xl6StdDirCopyBody[] =
    "func __xl_std_dir_copy(string source, string target, bool overwrite = true) as bool\n"
    "    if not __xl_std_xrt_dir_exists(source) then\n"
    "        return false\n"
    "    end\n"
    "    __xl_std_xrt_dir_copy(source, target, overwrite)\n"
    "    return __xl_std_xrt_dir_exists(target)\n"
    "end\n";

static const char xl6StdDirMoveBody[] =
    "func __xl_std_dir_move(string source, string target, bool overwrite = true) as bool\n"
    "    if not __xl_std_xrt_dir_exists(source) then\n"
    "        return false\n"
    "    end\n"
    "    __xl_std_xrt_dir_move(source, target, overwrite)\n"
    "    return (not __xl_std_xrt_dir_exists(source)) and __xl_std_xrt_dir_exists(target)\n"
    "end\n";

static const char xl6StdDirDeleteBody[] =
    "func __xl_std_dir_delete(string path) as bool\n"
    "    if not __xl_std_xrt_dir_exists(path) then\n"
    "        return false\n"
    "    end\n"
    "    __xl_std_xrt_dir_delete(path)\n"
    "    return not __xl_std_xrt_dir_exists(path)\n"
    "end\n";

static const char xl6StdDirListBody[] =
    "func __xl_std_dir_list(string path, bool recursive = false) as array\n"
    "    return array(__xl_std_runtime_dir_list(path, recursive, 0))\n"
    "end\n";

static const char xl6StdDirFilesBody[] =
    "func __xl_std_dir_files(string path, bool recursive = false) as array\n"
    "    return array(__xl_std_runtime_dir_list(path, recursive, 1))\n"
    "end\n";

static const char xl6StdDirDirsBody[] =
    "func __xl_std_dir_dirs(string path, bool recursive = false) as array\n"
    "    return array(__xl_std_runtime_dir_list(path, recursive, 2))\n"
    "end\n";

static const char xl6StdJsonParseBody[] =
    "func __xl_std_json_parse(string text) as var\n"
    "    return __xl_std_xrt_json_parse(text, 0)\n"
    "end\n";

static const char xl6StdJsonReadBody[] =
    "func __xl_std_json_read(string path) as var\n"
    "    return __xl_std_xrt_json_parse_file(path)\n"
    "end\n";

static const char xl6StdJsonStringifyBody[] =
    "func __xl_std_json_stringify(var value, bool pretty = false) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_json_stringify(value, int(pretty), null))\n"
    "end\n";

static const char xl6StdJsonWriteBody[] =
    "func __xl_std_json_write(string path, var value, bool pretty = false) as bool\n"
    "    return __xl_std_xrt_json_write_file(path, value, int(pretty)) != 0\n"
    "end\n";

static const char xl6StdXsonParseBody[] =
    "func __xl_std_xson_parse(string text) as var\n"
    "    return __xl_std_xrt_xson_parse(text, 0)\n"
    "end\n";

static const char xl6StdXsonReadBody[] =
    "func __xl_std_xson_read(string path) as var\n"
    "    return __xl_std_xrt_xson_parse_file(path)\n"
    "end\n";

static const char xl6StdXsonStringifyBody[] =
    "func __xl_std_xson_stringify(var value, bool pretty = false) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_xson_stringify(value, int(pretty), 0, null))\n"
    "end\n";

static const char xl6StdXsonWriteBody[] =
    "func __xl_std_xson_write(string path, var value, bool pretty = false) as bool\n"
    "    return __xl_std_xrt_xson_write_file(path, value, int(pretty), 0) != 0\n"
    "end\n";

static const char xl6StdCodecHexEncodeBody[] =
    "func __xl_std_codec_hex_encode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_hex_encode(text, 0))\n"
    "end\n";

static const char xl6StdCodecHexDecodeBody[] =
    "func __xl_std_codec_hex_decode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_hex_decode(text, 0))\n"
    "end\n";

static const char xl6StdCodecBase64EncodeBody[] =
    "func __xl_std_codec_base64_encode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_base64_encode(text, 0, null))\n"
    "end\n";

static const char xl6StdCodecBase64DecodeBody[] =
    "func __xl_std_codec_base64_decode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_base64_decode(text, 0, null))\n"
    "end\n";

static const char xl6StdHashU32Body[] =
    "func __xl_std_hash_u32(string text) as uint32\n"
    "    return __xl_std_xrt_hash32(text, __xl_std_crt_strlen(text))\n"
    "end\n";

static const char xl6StdHashU32WithSeedBody[] =
    "func __xl_std_hash_u32_with_seed(string text, uint32 seed) as uint32\n"
    "    return __xl_std_xrt_hash32_seed(text, __xl_std_crt_strlen(text), c_uint(seed))\n"
    "end\n";

static const char xl6StdHashU64Body[] =
    "func __xl_std_hash_u64(string text) as uint64\n"
    "    return __xl_std_xrt_hash64(text, __xl_std_crt_strlen(text))\n"
    "end\n";

static const char xl6StdHashU64WithSeedBody[] =
    "func __xl_std_hash_u64_with_seed(string text, uint64 seed) as uint64\n"
    "    return __xl_std_xrt_hash64_seed(text, __xl_std_crt_strlen(text), c_ulonglong(seed))\n"
    "end\n";

static const char xl6StdCryptoSha1Body[] =
    "func __xl_std_crypto_sha1(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_sha1_hex(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoSha256Body[] =
    "func __xl_std_crypto_sha256(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_sha256_hex(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoSha384Body[] =
    "func __xl_std_crypto_sha384(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_sha384_hex(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdCryptoSha512Body[] =
    "func __xl_std_crypto_sha512(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_sha512_hex(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdIdXidBody[] =
    "func __xl_std_id_xid() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_make_xids())\n"
    "end\n";

static const char xl6StdNetLocalIPBody[] =
    "func __xl_std_net_local_ip() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_get_local_ip())\n"
    "end\n";

static const char xl6StdNetLocalMACBody[] =
    "func __xl_std_net_local_mac() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_get_local_mac())\n"
    "end\n";

static const char xl6StdNetLocalNameBody[] =
    "func __xl_std_net_local_name() as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_xrt_get_local_name())\n"
    "end\n";

static const char xl6StdUrlValidBody[] =
    "func __xl_std_url_valid(string text) as bool\n"
    "    return __xl_std_runtime_url_flag(text, __xl_std_crt_strlen(text), 0)\n"
    "end\n";

static const char xl6StdUrlIsAbsoluteBody[] =
    "func __xl_std_url_is_absolute(string text) as bool\n"
    "    return __xl_std_runtime_url_flag(text, __xl_std_crt_strlen(text), 1)\n"
    "end\n";

static const char xl6StdUrlIsSecureBody[] =
    "func __xl_std_url_is_secure(string text) as bool\n"
    "    return __xl_std_runtime_url_flag(text, __xl_std_crt_strlen(text), 2)\n"
    "end\n";

static const char xl6StdUrlPortBody[] =
    "func __xl_std_url_port(string text) as int\n"
    "    return int(__xl_std_runtime_url_port(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdUrlSchemeBody[] =
    "func __xl_std_url_scheme(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 1))\n"
    "end\n";

static const char xl6StdUrlAuthorityBody[] =
    "func __xl_std_url_authority(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 2))\n"
    "end\n";

static const char xl6StdUrlUserInfoBody[] =
    "func __xl_std_url_user_info(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 3))\n"
    "end\n";

static const char xl6StdUrlHostBody[] =
    "func __xl_std_url_host(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 4))\n"
    "end\n";

static const char xl6StdUrlPathBody[] =
    "func __xl_std_url_path(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 5))\n"
    "end\n";

static const char xl6StdUrlQueryBody[] =
    "func __xl_std_url_query(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 6))\n"
    "end\n";

static const char xl6StdUrlFragmentBody[] =
    "func __xl_std_url_fragment(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 7))\n"
    "end\n";

static const char xl6StdUrlTargetBody[] =
    "func __xl_std_url_target(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_part(text, __xl_std_crt_strlen(text), 8))\n"
    "end\n";

static const char xl6StdUrlResolveBody[] =
    "func __xl_std_url_resolve(string base, string ref) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_resolve(base, __xl_std_crt_strlen(base), ref, __xl_std_crt_strlen(ref)))\n"
    "end\n";

static const char xl6StdUrlNormalizePathBody[] =
    "func __xl_std_url_normalize_path(string path) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_url_normalize_path(path, __xl_std_crt_strlen(path)))\n"
    "end\n";

static const char xl6StdQueryEncodeBody[] =
    "func __xl_std_query_encode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_query_encode(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdQueryDecodeBody[] =
    "func __xl_std_query_decode(string text) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_query_decode(text, __xl_std_crt_strlen(text)))\n"
    "end\n";

static const char xl6StdQueryValidBody[] =
    "func __xl_std_query_valid(string text) as bool\n"
    "    return __xl_std_runtime_query_valid(text, __xl_std_crt_strlen(text))\n"
    "end\n";

static const char xl6StdQueryPairBody[] =
    "func __xl_std_query_pair(string name, string value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_query_pair(name, __xl_std_crt_strlen(name), value, __xl_std_crt_strlen(value)))\n"
    "end\n";

static const char xl6StdQueryAppendBody[] =
    "func __xl_std_query_append(string query, string name, string value) as string\n"
    "    return __xl_std_runtime_take_cstr(__xl_std_runtime_query_append(query, __xl_std_crt_strlen(query), name, __xl_std_crt_strlen(name), value, __xl_std_crt_strlen(value)))\n"
    "end\n";

static const char xl6StdHttpServerCreateBody[] =
    "func __xl_std_http_server_create(int port, func(string, string, string, string) as string handler) as point\n"
    "    return __xl_std_runtime_http_server_create(c_int(port), handler)\n"
    "end\n";

static const char xl6StdHttpServerStartBody[] =
    "func __xl_std_http_server_start(point server) as bool\n"
    "    return __xl_std_runtime_http_server_start(server)\n"
    "end\n";

static const char xl6StdHttpServerPortBody[] =
    "func __xl_std_http_server_port(point server) as int\n"
    "    return int(__xl_std_runtime_http_server_port(server))\n"
    "end\n";

static const char xl6StdHttpServerStopBody[] =
    "func __xl_std_http_server_stop(point server)\n"
    "    __xl_std_runtime_http_server_stop(server)\n"
    "end\n";

static const char xl6StdHttpServerDestroyBody[] =
    "func __xl_std_http_server_destroy(point server)\n"
    "    __xl_std_runtime_http_server_destroy(server)\n"
    "end\n";

static const char xl6StdPrintBody[] =
    "func __xl_std_print(...)\n"
    "    __xl_std_runtime_print(arg, false)\n"
    "end\n";

static const char xl6StdPrintlnBody[] =
    "func __xl_std_println(...)\n"
    "    __xl_std_runtime_print(arg, true)\n"
    "end\n";

static const XL6_StdlibFunc xl6StdlibFuncs[] = {
    {"__crt_string", "strlen", "__xl_std_crt_strlen", "strlen", xl6StdStringLenParams, 1, "c_size", NULL},
    {"__xl_runtime", "takeCstr", "__xl_std_runtime_take_cstr", "xl6_xrt_take_cstr", xl6StdRuntimeTakeCstrParams, 1, "string", NULL},
    {"__xl_runtime", "print", "__xl_std_runtime_print", "xl6_xrt_print", xl6StdRuntimePrintParams, 2, "", NULL},
    {"__xl_runtime", "stringSplit", "__xl_std_runtime_string_split", "xl6_xrt_string_split", xl6StdRuntimeStringSplitParams, 2, "var", NULL},
    {"__xl_runtime", "stringStartsWith", "__xl_std_runtime_string_starts_with", "xl6_xrt_string_starts_with", xl6StdRuntimeStringEdgeParams, 5, "c_bool", NULL},
    {"__xl_runtime", "stringEndsWith", "__xl_std_runtime_string_ends_with", "xl6_xrt_string_ends_with", xl6StdRuntimeStringEdgeParams, 5, "c_bool", NULL},
    {"__xl_runtime", "sha1Hex", "__xl_std_runtime_sha1_hex", "xl6_xrt_sha1_hex", xl6StdRuntimeCryptoTextParams, 2, "point", NULL},
    {"__xl_runtime", "sha256Hex", "__xl_std_runtime_sha256_hex", "xl6_xrt_sha256_hex", xl6StdRuntimeCryptoTextParams, 2, "point", NULL},
    {"__xl_runtime", "sha384Hex", "__xl_std_runtime_sha384_hex", "xl6_xrt_sha384_hex", xl6StdRuntimeCryptoTextParams, 2, "point", NULL},
    {"__xl_runtime", "sha512Hex", "__xl_std_runtime_sha512_hex", "xl6_xrt_sha512_hex", xl6StdRuntimeCryptoTextParams, 2, "point", NULL},
    {"__xl_runtime", "urlPart", "__xl_std_runtime_url_part", "xl6_xrt_url_part", xl6StdRuntimeUrlPartParams, 3, "point", NULL},
    {"__xl_runtime", "urlFlag", "__xl_std_runtime_url_flag", "xl6_xrt_url_flag", xl6StdRuntimeUrlPartParams, 3, "c_bool", NULL},
    {"__xl_runtime", "urlPort", "__xl_std_runtime_url_port", "xl6_xrt_url_port", xl6StdRuntimeUrlTextParams, 2, "c_int", NULL},
    {"__xl_runtime", "urlResolve", "__xl_std_runtime_url_resolve", "xl6_xrt_url_resolve", xl6StdRuntimeUrlResolveParams, 4, "point", NULL},
    {"__xl_runtime", "urlNormalizePath", "__xl_std_runtime_url_normalize_path", "xl6_xrt_url_normalize_path", xl6StdRuntimeUrlTextParams, 2, "point", NULL},
    {"__xl_runtime", "queryEncode", "__xl_std_runtime_query_encode", "xl6_xrt_query_encode", xl6StdRuntimeQueryTextParams, 2, "point", NULL},
    {"__xl_runtime", "queryDecode", "__xl_std_runtime_query_decode", "xl6_xrt_query_decode", xl6StdRuntimeQueryTextParams, 2, "point", NULL},
    {"__xl_runtime", "queryValid", "__xl_std_runtime_query_valid", "xl6_xrt_query_valid", xl6StdRuntimeQueryTextParams, 2, "c_bool", NULL},
    {"__xl_runtime", "queryPair", "__xl_std_runtime_query_pair", "xl6_xrt_query_pair", xl6StdRuntimeQueryPairParams, 4, "point", NULL},
    {"__xl_runtime", "queryAppend", "__xl_std_runtime_query_append", "xl6_xrt_query_append", xl6StdRuntimeQueryAppendParams, 6, "point", NULL},
    {"__xl_runtime", "regexIsMatch", "__xl_std_runtime_regex_is_match", "xl6_xrt_regex_is_match", xl6StdRuntimeRegexTextParams, 3, "c_bool", NULL},
    {"__xl_runtime", "regexFind", "__xl_std_runtime_regex_find", "xl6_xrt_regex_find", xl6StdRuntimeRegexTextParams, 3, "c_ssize", NULL},
    {"__xl_runtime", "processExec", "__xl_std_runtime_process_exec", "xl6_xrt_process_exec", xl6StdRuntimeProcessExecParams, 2, "point", NULL},
    {"__xl_runtime", "processSuccess", "__xl_std_runtime_process_success", "xl6_xrt_process_success", xl6StdRuntimeProcessExecParams, 2, "c_bool", NULL},
    {"__xl_runtime", "processExitCode", "__xl_std_runtime_process_exit_code", "xl6_xrt_process_exit_code", xl6StdRuntimeProcessExecParams, 2, "c_int", NULL},
    {"__xl_runtime", "dirList", "__xl_std_runtime_dir_list", "xl6_xrt_dir_list", xl6StdRuntimeDirListParams, 3, "var", NULL},
    {"__xl_runtime", "pathJoin", "__xl_std_runtime_path_join", "xl6_xrt_path_join", xl6StdRuntimePathJoinParams, 4, "point", NULL},
    {"__xl_runtime", "httpServerCreate", "__xl_std_runtime_http_server_create", "xl6_xrt_http_server_create", xl6StdRuntimeHttpServerCreateParams, 2, "point", NULL},
    {"__xl_runtime", "httpServerStart", "__xl_std_runtime_http_server_start", "xl6_xrt_http_server_start", xl6StdRuntimeHttpServerHandleParams, 1, "c_bool", NULL},
    {"__xl_runtime", "httpServerPort", "__xl_std_runtime_http_server_port", "xl6_xrt_http_server_port", xl6StdRuntimeHttpServerHandleParams, 1, "c_int", NULL},
    {"__xl_runtime", "httpServerStop", "__xl_std_runtime_http_server_stop", "xl6_xrt_http_server_stop", xl6StdRuntimeHttpServerHandleParams, 1, "", NULL},
    {"__xl_runtime", "httpServerDestroy", "__xl_std_runtime_http_server_destroy", "xl6_xrt_http_server_destroy", xl6StdRuntimeHttpServerHandleParams, 1, "", NULL},

    {"__xrt_string", "strComp", "__xl_std_xrt_str_comp", "xrtStrComp", xl6StdXrtStrCompParams, 4, "c_int", NULL},
    {"__xrt_string", "inStr", "__xl_std_xrt_in_str", "xrtInStr", xl6StdXrtInStrParams, 5, "c_uint", NULL},
    {"__xrt_string", "checkStr", "__xl_std_xrt_check_str", "xrtCheckStr", xl6StdXrtCheckStrParams, 4, "point", NULL},
    {"__xrt_string", "strLike", "__xl_std_xrt_str_like", "xrtStrLike", xl6StdXrtStrLikeParams, 5, "c_bool", NULL},
    {"__xrt_string", "strApprox", "__xl_std_xrt_str_approx", "xrtStrApprox", xl6StdXrtStrApproxParams, 4, "c_bool", NULL},
    {"__xrt_string", "strSim", "__xl_std_xrt_str_sim", "xrtStrSim", xl6StdXrtStrApproxParams, 4, "c_double", NULL},
    {"__xrt_string", "isUtf8", "__xl_std_xrt_is_utf8", "xrtIsUTF8", xl6StdXrtIsUtf8Params, 2, "c_bool", NULL},
    {"__xrt_string", "intFormat", "__xl_std_xrt_int_format", "xrtIntFormat", xl6StdXrtIntFormatParams, 2, "point", NULL},
    {"__xrt_string", "numFormat", "__xl_std_xrt_num_format", "xrtNumFormat", xl6StdXrtNumFormatParams, 2, "point", NULL},
    {"__xrt_string", "lcase", "__xl_std_xrt_lcase", "xrtLCase", xl6StdXrtStringCaseParams, 3, "point", NULL},
    {"__xrt_string", "ucase", "__xl_std_xrt_ucase", "xrtUCase", xl6StdXrtStringCaseParams, 3, "point", NULL},
    {"__xrt_string", "trim", "__xl_std_xrt_trim", "xrtTrim", xl6StdXrtStringTrimParams, 6, "point", NULL},
    {"__xrt_string", "ltrim", "__xl_std_xrt_ltrim", "xrtLTrim", xl6StdXrtStringTrimParams, 6, "point", NULL},
    {"__xrt_string", "rtrim", "__xl_std_xrt_rtrim", "xrtRTrim", xl6StdXrtStringTrimParams, 6, "point", NULL},
    {"__xrt_string", "filter", "__xl_std_xrt_filter", "xrtFilterStr", xl6StdXrtStringTrimParams, 6, "point", NULL},
    {"__xrt_string", "replace", "__xl_std_xrt_replace", "xrtReplace", xl6StdXrtStringReplaceParams, 7, "point", NULL},
    {"__xrt_time", "dateAdd", "__xl_std_xrt_date_add", "xrtDateAdd", xl6StdXrtDateAddParams, 3, "int", NULL},
    {"__xrt_time", "dateDiff", "__xl_std_xrt_date_diff", "xrtDateDiff", xl6StdXrtDateDiffParams, 3, "int", NULL},
    {"__xrt_time", "strToTime", "__xl_std_xrt_str_to_time", "xrtStrToTime", xl6StdXrtStrToTimeParams, 2, "int", NULL},
    {"__xrt_time", "timeToStr", "__xl_std_xrt_time_to_str", "xrtTimeToStr", xl6StdXrtTimeToStrParams, 2, "point", NULL},
    {"__xrt_time", "timeFormat", "__xl_std_xrt_time_format", "xrtTimeFormat", xl6StdXrtTimeFormatParams, 2, "point", NULL},
    {"__xrt_time", "timeParse", "__xl_std_xrt_time_parse", "xrtTimeParse", xl6StdXrtTimeParseParams, 2, "int", NULL},
    {"__xrt_time", "dateTimeSerial", "__xl_std_xrt_datetime_serial", "xrtDateTimeSerial", xl6StdTimeFromDateTimeParams, 6, "int", NULL},
    {"__xrt_time", "now", "__xl_std_xrt_now", "xrtNow", NULL, 0, "int", NULL},
    {"__xrt_time", "date", "__xl_std_xrt_date", "xrtDate", NULL, 0, "int", NULL},
    {"__xrt_time", "time", "__xl_std_xrt_time", "xrtTime", NULL, 0, "int", NULL},
    {"__xrt_time", "timeSerial", "__xl_std_xrt_time_serial", "xrtTimeSerial", xl6StdTimeFromTimeParams, 3, "int", NULL},
    {"__xrt_time", "dateSerial", "__xl_std_xrt_date_serial", "xrtDateSerial", xl6StdTimeFromDateParams, 3, "int", NULL},
    {"__xrt_time", "year", "__xl_std_xrt_year", "xrtYear", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "month", "__xl_std_xrt_month", "xrtMonth", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "day", "__xl_std_xrt_day", "xrtDay", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "hour", "__xl_std_xrt_hour", "xrtHour", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "minute", "__xl_std_xrt_minute", "xrtMinute", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "second", "__xl_std_xrt_second", "xrtSecond", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "weekday", "__xl_std_xrt_weekday", "xrtWeekday", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "dayOfYear", "__xl_std_xrt_day_of_year", "xrtDayOfYear", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "quarter", "__xl_std_xrt_quarter", "xrtQuarter", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "isLeapYear", "__xl_std_xrt_is_leap_year", "xrtIsLeapYear", xl6StdXrtYearParams, 1, "c_bool", NULL},
    {"__xrt_time", "daysInMonth", "__xl_std_xrt_days_in_month", "xrtDaysInMonth", xl6StdXrtYearMonthParams, 2, "c_int", NULL},
    {"__xrt_time", "daysInYear", "__xl_std_xrt_days_in_year", "xrtDaysInYear", xl6StdXrtYearParams, 1, "c_int", NULL},
    {"__xrt_time", "datePart", "__xl_std_xrt_date_part", "xrtDatePart", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "timePart", "__xl_std_xrt_time_part", "xrtTimePart", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "isSameDay", "__xl_std_xrt_is_same_day", "xrtIsSameDay", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "isSameMonth", "__xl_std_xrt_is_same_month", "xrtIsSameMonth", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "isSameYear", "__xl_std_xrt_is_same_year", "xrtIsSameYear", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "inRange", "__xl_std_xrt_time_in_range", "xrtTimeInRange", xl6StdXrtIntInRangeParams, 3, "c_bool", NULL},
    {"__xrt_time", "timer", "__xl_std_xrt_timer", "xrtTimer", NULL, 0, "c_double", NULL},
    {"__xrt_time", "sleep", "__xl_std_xrt_sleep", "xrtSleep", xl6StdXrtSleepParams, 1, "", NULL},
    {"__xrt_time", "nowUTC", "__xl_std_xrt_now_utc", "xrtNowUTC", NULL, 0, "int", NULL},
    {"__xrt_time", "timezoneOffset", "__xl_std_xrt_timezone_offset", "xrtTimezoneOffset", NULL, 0, "c_int", NULL},
    {"__xrt_time", "rangeOverlap", "__xl_std_xrt_time_range_overlap", "xrtTimeRangeOverlap", xl6StdXrtTimeOverlapParams, 4, "c_bool", NULL},
    {"__xrt_time", "approx", "__xl_std_xrt_time_approx", "xrtTimeApprox", xl6StdXrtIntBinaryParams, 2, "c_bool", NULL},
    {"__xrt_time", "toUnix", "__xl_std_xrt_time_to_unix", "xrtToUnixTime", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "fromUnix", "__xl_std_xrt_time_from_unix", "xrtFromUnixTime", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "utcToLocal", "__xl_std_xrt_utc_to_local", "xrtUTCToLocal", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "localToUTC", "__xl_std_xrt_local_to_utc", "xrtLocalToUTC", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "firstDayOfMonth", "__xl_std_xrt_first_day_of_month", "xrtFirstDayOfMonth", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "lastDayOfMonth", "__xl_std_xrt_last_day_of_month", "xrtLastDayOfMonth", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "firstDayOfYear", "__xl_std_xrt_first_day_of_year", "xrtFirstDayOfYear", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "lastDayOfYear", "__xl_std_xrt_last_day_of_year", "xrtLastDayOfYear", xl6StdXrtIntUnaryParams, 1, "int", NULL},
    {"__xrt_time", "firstDayOfWeek", "__xl_std_xrt_first_day_of_week", "xrtFirstDayOfWeek", xl6StdXrtWeekBoundaryParams, 2, "int", NULL},
    {"__xrt_time", "lastDayOfWeek", "__xl_std_xrt_last_day_of_week", "xrtLastDayOfWeek", xl6StdXrtWeekBoundaryParams, 2, "int", NULL},
    {"__xrt_time", "weekOfYear", "__xl_std_xrt_week_of_year", "xrtWeekOfYear", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_time", "weekOfMonth", "__xl_std_xrt_week_of_month", "xrtWeekOfMonth", xl6StdXrtIntUnaryParams, 1, "c_int", NULL},
    {"__xrt_random", "rand32", "__xl_std_xrt_rand32", "xrtRand32", NULL, 0, "c_uint", NULL},
    {"__xrt_random", "rand64", "__xl_std_xrt_rand64", "xrtRand64", NULL, 0, "c_ulonglong", NULL},
    {"__xrt_random", "range", "__xl_std_xrt_rand_range", "xrtRandRange", xl6StdXrtRandRangeParams, 2, "c_int", NULL},
    {"__xrt_random", "string", "__xl_std_xrt_rand_str", "xrtRandStr", xl6StdXrtRandStrParams, 3, "point", NULL},
    {"__xrt_path", "exists", "__xl_std_xrt_path_exists", "xrtPathExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_path", "isAbs", "__xl_std_xrt_path_is_abs", "xrtPathIsAbs", xl6StdXrtPathIsAbsParams, 2, "c_bool", NULL},
    {"__xrt_path", "getNameExt", "__xl_std_xrt_path_get_name_ext", "xrtPathGetNameExt", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "getName", "__xl_std_xrt_path_get_name", "xrtPathGetName", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "getExt", "__xl_std_xrt_path_get_ext", "xrtPathGetExt", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "getDir", "__xl_std_xrt_path_get_dir", "xrtPathGetDir", xl6StdXrtPathIsAbsParams, 2, "point", NULL},
    {"__xrt_path", "random", "__xl_std_xrt_path_random", "xrtPathRandom", xl6StdXrtPathRandomParams, 5, "point", NULL},
    {"__xrt_file", "exists", "__xl_std_xrt_file_exists", "xrtFileExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_file", "getSize", "__xl_std_xrt_file_get_size", "xrtFileGetSize", xl6StdPathUnaryParams, 1, "c_size", NULL},
    {"__xrt_file", "setSize", "__xl_std_xrt_file_set_size", "xrtFileSetSize", xl6StdXrtFileSetSizeParams, 2, "c_bool", NULL},
    {"__xrt_file", "getAttr", "__xl_std_xrt_file_get_attr", "xrtFileGetAttr", xl6StdPathUnaryParams, 1, "c_int", NULL},
    {"__xrt_file", "setAttr", "__xl_std_xrt_file_set_attr", "xrtFileSetAttr", xl6StdXrtFileSetAttrParams, 2, "c_bool", NULL},
    {"__xrt_file", "getAccessTime", "__xl_std_xrt_file_get_access_time", "xrtFileGetAccessTime", xl6StdPathUnaryParams, 1, "int", NULL},
    {"__xrt_file", "getChangeTime", "__xl_std_xrt_file_get_change_time", "xrtFileGetChangeTime", xl6StdPathUnaryParams, 1, "int", NULL},
    {"__xrt_file", "writeAll", "__xl_std_xrt_file_write_all", "xrtFileWriteAll", xl6StdXrtFileWriteTextParams, 4, "c_int", NULL},
    {"__xrt_file", "append", "__xl_std_xrt_file_append", "xrtFileAppend", xl6StdXrtFileWriteTextParams, 4, "c_int", NULL},
    {"__xrt_file", "readAll", "__xl_std_xrt_file_read_all", "xrtFileReadAll", xl6StdXrtFileReadTextParams, 3, "point", NULL},
    {"__xrt_file", "delete", "__xl_std_xrt_file_delete", "xrtFileDelete", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_file", "copy", "__xl_std_xrt_file_copy", "xrtFileCopy", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_file", "move", "__xl_std_xrt_file_move", "xrtFileMove", xl6StdXrtFileCopyMoveParams, 3, "c_bool", NULL},
    {"__xrt_dir", "exists", "__xl_std_xrt_dir_exists", "xrtDirExists", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "create", "__xl_std_xrt_dir_create", "xrtDirCreate", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "createAll", "__xl_std_xrt_dir_create_all", "xrtDirCreateAll", xl6StdPathUnaryParams, 1, "c_bool", NULL},
    {"__xrt_dir", "copy", "__xl_std_xrt_dir_copy", "xrtDirCopy", xl6StdXrtFileCopyMoveParams, 3, "c_int", NULL},
    {"__xrt_dir", "move", "__xl_std_xrt_dir_move", "xrtDirMove", xl6StdXrtFileCopyMoveParams, 3, "c_int", NULL},
    {"__xrt_dir", "delete", "__xl_std_xrt_dir_delete", "xrtDirDelete", xl6StdPathUnaryParams, 1, "c_int", NULL},
    {"__xrt_json", "parse", "__xl_std_xrt_json_parse", "xrtParseJSON", xl6StdXrtJsonParseParams, 2, "var", NULL},
    {"__xrt_json", "parseFile", "__xl_std_xrt_json_parse_file", "xrtParseJSON_File", xl6StdPathUnaryParams, 1, "var", NULL},
    {"__xrt_json", "stringify", "__xl_std_xrt_json_stringify", "xrtStringifyJSON", xl6StdXrtJsonStringifyParams, 3, "point", NULL},
    {"__xrt_json", "writeFile", "__xl_std_xrt_json_write_file", "xrtStringifyJSON_File", xl6StdXrtJsonWriteParams, 3, "c_int", NULL},
    {"__xrt_xson", "parse", "__xl_std_xrt_xson_parse", "xrtParseXSON", xl6StdXrtJsonParseParams, 2, "var", NULL},
    {"__xrt_xson", "parseFile", "__xl_std_xrt_xson_parse_file", "xrtParseXSON_File", xl6StdPathUnaryParams, 1, "var", NULL},
    {"__xrt_xson", "stringify", "__xl_std_xrt_xson_stringify", "xrtStringifyXSON", xl6StdXrtXsonStringifyParams, 4, "point", NULL},
    {"__xrt_xson", "writeFile", "__xl_std_xrt_xson_write_file", "xrtStringifyXSON_File", xl6StdXrtXsonWriteParams, 4, "c_int", NULL},
    {"__xrt_codec", "hexEncode", "__xl_std_xrt_hex_encode", "xrtHexEncode", xl6StdXrtCodecTextParams, 2, "point", NULL},
    {"__xrt_codec", "hexDecode", "__xl_std_xrt_hex_decode", "xrtHexDecode", xl6StdXrtCodecTextParams, 2, "point", NULL},
    {"__xrt_codec", "base64Encode", "__xl_std_xrt_base64_encode", "xrtBase64Encode", xl6StdXrtCodecBase64Params, 3, "point", NULL},
    {"__xrt_codec", "base64Decode", "__xl_std_xrt_base64_decode", "xrtBase64Decode", xl6StdXrtCodecBase64Params, 3, "point", NULL},
    {"__xrt_hash", "u32", "__xl_std_xrt_hash32", "xrtHash32", xl6StdXrtHashParams, 2, "c_uint", NULL},
    {"__xrt_hash", "u32WithSeed", "__xl_std_xrt_hash32_seed", "xrtHash32_WithSeed", xl6StdXrtHash32SeedParams, 3, "c_uint", NULL},
    {"__xrt_hash", "u64", "__xl_std_xrt_hash64", "xrtHash64", xl6StdXrtHashParams, 2, "c_ulonglong", NULL},
    {"__xrt_hash", "u64WithSeed", "__xl_std_xrt_hash64_seed", "xrtHash64_WithSeed", xl6StdXrtHash64SeedParams, 3, "c_ulonglong", NULL},
    {"__xrt_id", "makeXIDS", "__xl_std_xrt_make_xids", "xrtMakeXIDS", NULL, 0, "point", NULL},
    {"__xrt_net", "localIP", "__xl_std_xrt_get_local_ip", "xrtGetLocalIP", NULL, 0, "point", NULL},
    {"__xrt_net", "localMAC", "__xl_std_xrt_get_local_mac", "xrtGetLocalMAC", NULL, 0, "point", NULL},
    {"__xrt_net", "localName", "__xl_std_xrt_get_local_name", "xrtGetLocalName", NULL, 0, "point", NULL},

    {"", "print", "__xl_std_print", NULL, NULL, 0, "", xl6StdPrintBody},
    {"", "println", "__xl_std_println", NULL, NULL, 0, "", xl6StdPrintlnBody},

    {"math", "sin", "__xl_std_math_sin", "sin", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "cos", "__xl_std_math_cos", "cos", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "tan", "__xl_std_math_tan", "tan", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "asin", "__xl_std_math_asin", "asin", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "acos", "__xl_std_math_acos", "acos", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "atan", "__xl_std_math_atan", "atan", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "atan2", "__xl_std_math_atan2", "atan2", xl6StdMathAtan2Params, 2, "float", NULL},
    {"math", "sqrt", "__xl_std_math_sqrt", "sqrt", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "pow", "__xl_std_math_pow", "pow", xl6StdMathPowParams, 2, "float", NULL},
    {"math", "exp", "__xl_std_math_exp", "exp", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "log", "__xl_std_math_log", "log", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "log10", "__xl_std_math_log10", "log10", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "floor", "__xl_std_math_floor", "floor", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "ceil", "__xl_std_math_ceil", "ceil", xl6StdMathUnaryFloatParams, 1, "float", NULL},
    {"math", "round", "__xl_std_math_round", NULL, xl6StdMathUnaryFloatParams, 1, "float", xl6StdMathRoundBody},
    {"math", "abs", "__xl_std_math_abs", "fabs", xl6StdMathUnaryFloatParams, 1, "float", NULL},

    {"string", "len", "__xl_std_string_len", NULL, NULL, 0, "int", xl6StdStringLenBody},
    {"string", "compare", "__xl_std_string_compare", NULL, NULL, 0, "int", xl6StdStringCompareBody},
    {"string", "equals", "__xl_std_string_equals", NULL, NULL, 0, "bool", xl6StdStringEqualsBody},
    {"string", "contains", "__xl_std_string_contains", NULL, NULL, 0, "bool", xl6StdStringContainsBody},
    {"string", "indexOf", "__xl_std_string_index_of", NULL, NULL, 0, "int", xl6StdStringIndexOfBody},
    {"string", "startsWith", "__xl_std_string_starts_with", NULL, NULL, 0, "bool", xl6StdStringStartsWithBody},
    {"string", "endsWith", "__xl_std_string_ends_with", NULL, NULL, 0, "bool", xl6StdStringEndsWithBody},
    {"string", "hasAny", "__xl_std_string_has_any", NULL, NULL, 0, "bool", xl6StdStringHasAnyBody},
    {"string", "like", "__xl_std_string_like", NULL, NULL, 0, "bool", xl6StdStringLikeBody},
    {"string", "approx", "__xl_std_string_approx", NULL, NULL, 0, "bool", xl6StdStringApproxBody},
    {"string", "sim", "__xl_std_string_sim", NULL, NULL, 0, "float", xl6StdStringSimBody},
    {"string", "isUtf8", "__xl_std_string_is_utf8", NULL, NULL, 0, "bool", xl6StdStringIsUtf8Body},
    {"string", "formatInt", "__xl_std_string_format_int", NULL, NULL, 0, "string", xl6StdStringFormatIntBody},
    {"string", "formatFloat", "__xl_std_string_format_float", NULL, NULL, 0, "string", xl6StdStringFormatFloatBody},
    {"string", "lower", "__xl_std_string_lower", NULL, NULL, 0, "string", xl6StdStringLowerBody},
    {"string", "upper", "__xl_std_string_upper", NULL, NULL, 0, "string", xl6StdStringUpperBody},
    {"string", "trim", "__xl_std_string_trim", NULL, NULL, 0, "string", xl6StdStringTrimBody},
    {"string", "ltrim", "__xl_std_string_ltrim", NULL, NULL, 0, "string", xl6StdStringLTrimBody},
    {"string", "rtrim", "__xl_std_string_rtrim", NULL, NULL, 0, "string", xl6StdStringRTrimBody},
    {"string", "filter", "__xl_std_string_filter", NULL, NULL, 0, "string", xl6StdStringFilterBody},
    {"string", "replace", "__xl_std_string_replace", NULL, NULL, 0, "string", xl6StdStringReplaceBody},
    {"string", "split", "__xl_std_string_split", NULL, NULL, 0, "array", xl6StdStringSplitBody},

    {"time", "now", "__xl_std_time_now", NULL, NULL, 0, "time", xl6StdTimeNowBody},
    {"time", "today", "__xl_std_time_today", NULL, NULL, 0, "time", xl6StdTimeTodayBody},
    {"time", "clock", "__xl_std_time_clock", NULL, NULL, 0, "time", xl6StdTimeClockBody},
    {"time", "fromTime", "__xl_std_time_from_time", NULL, NULL, 0, "time", xl6StdTimeFromTimeBody},
    {"time", "fromDate", "__xl_std_time_from_date", NULL, NULL, 0, "time", xl6StdTimeFromDateBody},
    {"time", "fromDateTime", "__xl_std_time_from_datetime", NULL, NULL, 0, "time", xl6StdTimeFromDateTimeBody},
    {"time", "parse", "__xl_std_time_parse", NULL, NULL, 0, "time", xl6StdTimeParseBody},
    {"time", "parsePattern", "__xl_std_time_parse_pattern", NULL, NULL, 0, "time", xl6StdTimeParsePatternBody},
    {"time", "year", "__xl_std_time_year", NULL, NULL, 0, "int", xl6StdTimeYearBody},
    {"time", "month", "__xl_std_time_month", NULL, NULL, 0, "int", xl6StdTimeMonthBody},
    {"time", "day", "__xl_std_time_day", NULL, NULL, 0, "int", xl6StdTimeDayBody},
    {"time", "hour", "__xl_std_time_hour", NULL, NULL, 0, "int", xl6StdTimeHourBody},
    {"time", "minute", "__xl_std_time_minute", NULL, NULL, 0, "int", xl6StdTimeMinuteBody},
    {"time", "second", "__xl_std_time_second", NULL, NULL, 0, "int", xl6StdTimeSecondBody},
    {"time", "weekday", "__xl_std_time_weekday", NULL, NULL, 0, "int", xl6StdTimeWeekdayBody},
    {"time", "dayOfYear", "__xl_std_time_day_of_year", NULL, NULL, 0, "int", xl6StdTimeDayOfYearBody},
    {"time", "quarter", "__xl_std_time_quarter", NULL, NULL, 0, "int", xl6StdTimeQuarterBody},
    {"time", "isLeapYear", "__xl_std_time_is_leap_year", NULL, NULL, 0, "bool", xl6StdTimeIsLeapYearBody},
    {"time", "daysInMonth", "__xl_std_time_days_in_month", NULL, NULL, 0, "int", xl6StdTimeDaysInMonthBody},
    {"time", "daysInYear", "__xl_std_time_days_in_year", NULL, NULL, 0, "int", xl6StdTimeDaysInYearBody},
    {"time", "datePart", "__xl_std_time_date_part", NULL, NULL, 0, "time", xl6StdTimeDatePartBody},
    {"time", "timePart", "__xl_std_time_time_part", NULL, NULL, 0, "time", xl6StdTimeTimePartBody},
    {"time", "isSameDay", "__xl_std_time_is_same_day", NULL, NULL, 0, "bool", xl6StdTimeIsSameDayBody},
    {"time", "isSameMonth", "__xl_std_time_is_same_month", NULL, NULL, 0, "bool", xl6StdTimeIsSameMonthBody},
    {"time", "isSameYear", "__xl_std_time_is_same_year", NULL, NULL, 0, "bool", xl6StdTimeIsSameYearBody},
    {"time", "inRange", "__xl_std_time_in_range", NULL, NULL, 0, "bool", xl6StdTimeInRangeBody},
    {"time", "addYears", "__xl_std_time_add_years", NULL, NULL, 0, "time", xl6StdTimeAddYearsBody},
    {"time", "addMonths", "__xl_std_time_add_months", NULL, NULL, 0, "time", xl6StdTimeAddMonthsBody},
    {"time", "addDays", "__xl_std_time_add_days", NULL, NULL, 0, "time", xl6StdTimeAddDaysBody},
    {"time", "addHours", "__xl_std_time_add_hours", NULL, NULL, 0, "time", xl6StdTimeAddHoursBody},
    {"time", "addMinutes", "__xl_std_time_add_minutes", NULL, NULL, 0, "time", xl6StdTimeAddMinutesBody},
    {"time", "addSeconds", "__xl_std_time_add_seconds", NULL, NULL, 0, "time", xl6StdTimeAddSecondsBody},
    {"time", "diffDays", "__xl_std_time_diff_days", NULL, NULL, 0, "int", xl6StdTimeDiffDaysBody},
    {"time", "timer", "__xl_std_time_timer", NULL, NULL, 0, "float", xl6StdTimeTimerBody},
    {"time", "sleep", "__xl_std_time_sleep", NULL, xl6StdTimeSleepParams, 1, "", xl6StdTimeSleepBody},
    {"time", "nowUTC", "__xl_std_time_now_utc", NULL, NULL, 0, "time", xl6StdTimeNowUtcBody},
    {"time", "timezoneOffset", "__xl_std_time_timezone_offset", NULL, NULL, 0, "int", xl6StdTimeTimezoneOffsetBody},
    {"time", "rangeOverlap", "__xl_std_time_range_overlap", NULL, NULL, 0, "bool", xl6StdTimeRangeOverlapBody},
    {"time", "approx", "__xl_std_time_approx", NULL, NULL, 0, "bool", xl6StdTimeApproxBody},
    {"time", "toUnix", "__xl_std_time_to_unix", NULL, NULL, 0, "int", xl6StdTimeToUnixBody},
    {"time", "fromUnix", "__xl_std_time_from_unix", NULL, NULL, 0, "time", xl6StdTimeFromUnixBody},
    {"time", "utcToLocal", "__xl_std_time_utc_to_local", NULL, NULL, 0, "time", xl6StdTimeUtcToLocalBody},
    {"time", "localToUTC", "__xl_std_time_local_to_utc", NULL, NULL, 0, "time", xl6StdTimeLocalToUtcBody},
    {"time", "firstDayOfMonth", "__xl_std_time_first_day_of_month", NULL, NULL, 0, "time", xl6StdTimeFirstDayOfMonthBody},
    {"time", "lastDayOfMonth", "__xl_std_time_last_day_of_month", NULL, NULL, 0, "time", xl6StdTimeLastDayOfMonthBody},
    {"time", "firstDayOfYear", "__xl_std_time_first_day_of_year", NULL, NULL, 0, "time", xl6StdTimeFirstDayOfYearBody},
    {"time", "lastDayOfYear", "__xl_std_time_last_day_of_year", NULL, NULL, 0, "time", xl6StdTimeLastDayOfYearBody},
    {"time", "firstDayOfWeek", "__xl_std_time_first_day_of_week", NULL, NULL, 0, "time", xl6StdTimeFirstDayOfWeekBody},
    {"time", "lastDayOfWeek", "__xl_std_time_last_day_of_week", NULL, NULL, 0, "time", xl6StdTimeLastDayOfWeekBody},
    {"time", "weekOfYear", "__xl_std_time_week_of_year", NULL, NULL, 0, "int", xl6StdTimeWeekOfYearBody},
    {"time", "weekOfMonth", "__xl_std_time_week_of_month", NULL, NULL, 0, "int", xl6StdTimeWeekOfMonthBody},
    {"time", "format", "__xl_std_time_format", NULL, NULL, 0, "string", xl6StdTimeFormatBody},
    {"time", "formatPattern", "__xl_std_time_format_pattern", NULL, NULL, 0, "string", xl6StdTimeFormatPatternBody},
    {"time", "dateString", "__xl_std_time_date_string", NULL, NULL, 0, "string", xl6StdTimeDateStringBody},
    {"time", "timeString", "__xl_std_time_time_string", NULL, NULL, 0, "string", xl6StdTimeTimeStringBody},

    {"random", "u32", "__xl_std_random_u32", NULL, NULL, 0, "uint32", xl6StdRandomU32Body},
    {"random", "u64", "__xl_std_random_u64", NULL, NULL, 0, "uint64", xl6StdRandomU64Body},
    {"random", "range", "__xl_std_random_range", NULL, NULL, 0, "int", xl6StdRandomRangeBody},
    {"random", "string", "__xl_std_random_string", NULL, NULL, 0, "string", xl6StdRandomStringBody},

    {"path", "exists", "__xl_std_path_exists", NULL, NULL, 0, "bool", xl6StdPathExistsBody},
    {"path", "isAbs", "__xl_std_path_is_abs", NULL, NULL, 0, "bool", xl6StdPathIsAbsBody},
    {"path", "nameExt", "__xl_std_path_name_ext", NULL, NULL, 0, "string", xl6StdPathNameExtBody},
    {"path", "name", "__xl_std_path_name", NULL, NULL, 0, "string", xl6StdPathNameBody},
    {"path", "ext", "__xl_std_path_ext", NULL, NULL, 0, "string", xl6StdPathExtBody},
    {"path", "dir", "__xl_std_path_dir", NULL, NULL, 0, "string", xl6StdPathDirBody},
    {"path", "join", "__xl_std_path_join", NULL, NULL, 0, "string", xl6StdPathJoinBody},
    {"path", "random", "__xl_std_path_random", NULL, NULL, 0, "string", xl6StdPathRandomBody},

    {"file", "exists", "__xl_std_file_exists", NULL, NULL, 0, "bool", xl6StdFileExistsBody},
    {"file", "size", "__xl_std_file_size", NULL, NULL, 0, "int", xl6StdFileSizeBody},
    {"file", "setSize", "__xl_std_file_set_size", NULL, NULL, 0, "bool", xl6StdFileSetSizeBody},
    {"file", "attr", "__xl_std_file_attr", NULL, NULL, 0, "int", xl6StdFileAttrBody},
    {"file", "setAttr", "__xl_std_file_set_attr", NULL, NULL, 0, "bool", xl6StdFileSetAttrBody},
    {"file", "accessTime", "__xl_std_file_access_time", NULL, NULL, 0, "time", xl6StdFileAccessTimeBody},
    {"file", "changeTime", "__xl_std_file_change_time", NULL, NULL, 0, "time", xl6StdFileChangeTimeBody},
    {"file", "readText", "__xl_std_file_read_text", NULL, xl6StdFileReadTextParams, 2, "string", xl6StdFileReadTextBody},
    {"file", "writeText", "__xl_std_file_write_text", NULL, NULL, 0, "bool", xl6StdFileWriteTextBody},
    {"file", "appendText", "__xl_std_file_append_text", NULL, NULL, 0, "bool", xl6StdFileAppendTextBody},
    {"file", "delete", "__xl_std_file_delete", NULL, NULL, 0, "bool", xl6StdFileDeleteBody},
    {"file", "copy", "__xl_std_file_copy", NULL, NULL, 0, "bool", xl6StdFileCopyBody},
    {"file", "move", "__xl_std_file_move", NULL, NULL, 0, "bool", xl6StdFileMoveBody},

    {"dir", "exists", "__xl_std_dir_exists", NULL, NULL, 0, "bool", xl6StdDirExistsBody},
    {"dir", "create", "__xl_std_dir_create", NULL, NULL, 0, "bool", xl6StdDirCreateBody},
    {"dir", "createAll", "__xl_std_dir_create_all", NULL, NULL, 0, "bool", xl6StdDirCreateAllBody},
    {"dir", "copy", "__xl_std_dir_copy", NULL, NULL, 0, "bool", xl6StdDirCopyBody},
    {"dir", "move", "__xl_std_dir_move", NULL, NULL, 0, "bool", xl6StdDirMoveBody},
    {"dir", "delete", "__xl_std_dir_delete", NULL, NULL, 0, "bool", xl6StdDirDeleteBody},
    {"dir", "list", "__xl_std_dir_list", NULL, NULL, 0, "array", xl6StdDirListBody},
    {"dir", "files", "__xl_std_dir_files", NULL, NULL, 0, "array", xl6StdDirFilesBody},
    {"dir", "dirs", "__xl_std_dir_dirs", NULL, NULL, 0, "array", xl6StdDirDirsBody},

    {"json", "parse", "__xl_std_json_parse", NULL, NULL, 0, "var", xl6StdJsonParseBody},
    {"json", "read", "__xl_std_json_read", NULL, NULL, 0, "var", xl6StdJsonReadBody},
    {"json", "stringify", "__xl_std_json_stringify", NULL, xl6StdJsonStringifyParams, 2, "string", xl6StdJsonStringifyBody},
    {"json", "write", "__xl_std_json_write", NULL, xl6StdJsonWriteParams, 3, "bool", xl6StdJsonWriteBody},

    {"xson", "parse", "__xl_std_xson_parse", NULL, NULL, 0, "var", xl6StdXsonParseBody},
    {"xson", "read", "__xl_std_xson_read", NULL, NULL, 0, "var", xl6StdXsonReadBody},
    {"xson", "stringify", "__xl_std_xson_stringify", NULL, xl6StdJsonStringifyParams, 2, "string", xl6StdXsonStringifyBody},
    {"xson", "write", "__xl_std_xson_write", NULL, xl6StdJsonWriteParams, 3, "bool", xl6StdXsonWriteBody},

    {"codec", "hexEncode", "__xl_std_codec_hex_encode", NULL, NULL, 0, "string", xl6StdCodecHexEncodeBody},
    {"codec", "hexDecode", "__xl_std_codec_hex_decode", NULL, NULL, 0, "string", xl6StdCodecHexDecodeBody},
    {"codec", "base64Encode", "__xl_std_codec_base64_encode", NULL, NULL, 0, "string", xl6StdCodecBase64EncodeBody},
    {"codec", "base64Decode", "__xl_std_codec_base64_decode", NULL, NULL, 0, "string", xl6StdCodecBase64DecodeBody},

    {"hash", "u32", "__xl_std_hash_u32", NULL, NULL, 0, "uint32", xl6StdHashU32Body},
    {"hash", "u32WithSeed", "__xl_std_hash_u32_with_seed", NULL, NULL, 0, "uint32", xl6StdHashU32WithSeedBody},
    {"hash", "u64", "__xl_std_hash_u64", NULL, NULL, 0, "uint64", xl6StdHashU64Body},
    {"hash", "u64WithSeed", "__xl_std_hash_u64_with_seed", NULL, NULL, 0, "uint64", xl6StdHashU64WithSeedBody},

    {"crypto", "sha1", "__xl_std_crypto_sha1", NULL, NULL, 0, "string", xl6StdCryptoSha1Body},
    {"crypto", "sha256", "__xl_std_crypto_sha256", NULL, NULL, 0, "string", xl6StdCryptoSha256Body},
    {"crypto", "sha384", "__xl_std_crypto_sha384", NULL, NULL, 0, "string", xl6StdCryptoSha384Body},
    {"crypto", "sha512", "__xl_std_crypto_sha512", NULL, NULL, 0, "string", xl6StdCryptoSha512Body},

    {"id", "xid", "__xl_std_id_xid", NULL, NULL, 0, "string", xl6StdIdXidBody},

    {"net", "localIP", "__xl_std_net_local_ip", NULL, NULL, 0, "string", xl6StdNetLocalIPBody},
    {"net", "localMAC", "__xl_std_net_local_mac", NULL, NULL, 0, "string", xl6StdNetLocalMACBody},
    {"net", "localName", "__xl_std_net_local_name", NULL, NULL, 0, "string", xl6StdNetLocalNameBody},

    {"url", "valid", "__xl_std_url_valid", NULL, NULL, 0, "bool", xl6StdUrlValidBody},
    {"url", "isAbsolute", "__xl_std_url_is_absolute", NULL, NULL, 0, "bool", xl6StdUrlIsAbsoluteBody},
    {"url", "isSecure", "__xl_std_url_is_secure", NULL, NULL, 0, "bool", xl6StdUrlIsSecureBody},
    {"url", "port", "__xl_std_url_port", NULL, NULL, 0, "int", xl6StdUrlPortBody},
    {"url", "scheme", "__xl_std_url_scheme", NULL, NULL, 0, "string", xl6StdUrlSchemeBody},
    {"url", "authority", "__xl_std_url_authority", NULL, NULL, 0, "string", xl6StdUrlAuthorityBody},
    {"url", "userInfo", "__xl_std_url_user_info", NULL, NULL, 0, "string", xl6StdUrlUserInfoBody},
    {"url", "host", "__xl_std_url_host", NULL, NULL, 0, "string", xl6StdUrlHostBody},
    {"url", "path", "__xl_std_url_path", NULL, NULL, 0, "string", xl6StdUrlPathBody},
    {"url", "query", "__xl_std_url_query", NULL, NULL, 0, "string", xl6StdUrlQueryBody},
    {"url", "fragment", "__xl_std_url_fragment", NULL, NULL, 0, "string", xl6StdUrlFragmentBody},
    {"url", "target", "__xl_std_url_target", NULL, NULL, 0, "string", xl6StdUrlTargetBody},
    {"url", "resolve", "__xl_std_url_resolve", NULL, NULL, 0, "string", xl6StdUrlResolveBody},
    {"url", "normalizePath", "__xl_std_url_normalize_path", NULL, NULL, 0, "string", xl6StdUrlNormalizePathBody},

    {"query", "encode", "__xl_std_query_encode", NULL, NULL, 0, "string", xl6StdQueryEncodeBody},
    {"query", "decode", "__xl_std_query_decode", NULL, NULL, 0, "string", xl6StdQueryDecodeBody},
    {"query", "valid", "__xl_std_query_valid", NULL, NULL, 0, "bool", xl6StdQueryValidBody},
    {"query", "pair", "__xl_std_query_pair", NULL, NULL, 0, "string", xl6StdQueryPairBody},
    {"query", "append", "__xl_std_query_append", NULL, NULL, 0, "string", xl6StdQueryAppendBody},

    {"regex", "isMatch", "__xl_std_regex_is_match", NULL, NULL, 0, "bool", xl6StdRegexIsMatchBody},
    {"regex", "find", "__xl_std_regex_find", NULL, NULL, 0, "int", xl6StdRegexFindBody},

    {"process", "exec", "__xl_std_process_exec", NULL, NULL, 0, "string", xl6StdProcessExecBody},
    {"process", "success", "__xl_std_process_success", NULL, NULL, 0, "bool", xl6StdProcessSuccessBody},
    {"process", "exitCode", "__xl_std_process_exit_code", NULL, NULL, 0, "int", xl6StdProcessExitCodeBody},

    {"http.server", "create", "__xl_std_http_server_create", NULL, NULL, 0, "point", xl6StdHttpServerCreateBody},
    {"http.server", "start", "__xl_std_http_server_start", NULL, NULL, 0, "bool", xl6StdHttpServerStartBody},
    {"http.server", "port", "__xl_std_http_server_port", NULL, NULL, 0, "int", xl6StdHttpServerPortBody},
    {"http.server", "stop", "__xl_std_http_server_stop", NULL, NULL, 0, "", xl6StdHttpServerStopBody},
    {"http.server", "destroy", "__xl_std_http_server_destroy", NULL, NULL, 0, "", xl6StdHttpServerDestroyBody}
};

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

    for ( i = 0; i < XL6_ARRAY_COUNT(xl6StdlibFuncs); ++i ) {
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

static int xl6StdlibNamespaceExists(const char* ns)
{
    size_t i;

    for ( i = 0; i < XL6_ARRAY_COUNT(xl6StdlibFuncs); ++i ) {
        if ( xl6StdlibTextEquals(xl6StdlibFuncs[i].ns, ns) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6StdlibMarkFuncUsed(int* used, const char* ns, const char* name)
{
    size_t index;

    if ( used == NULL ) {
        return;
    }
    if ( xl6StdlibFindFunc(ns, name, &index) != NULL ) {
        used[index] = 1;
    }
}

static void xl6StdlibApplyDeps(int* used)
{
    size_t index;

    if ( used == NULL ) {
        return;
    }

    /*
        round 在旧 MSVCRT/TCC 环境没有稳定符号。
        这里把它实现为 xlang helper，并自动注入 floor/ceil 依赖。
    */
    if ( xl6StdlibFindFunc("math", "round", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "math", "floor");
        xl6StdlibMarkFuncUsed(used, "math", "ceil");
    }

    if ( xl6StdlibFindFunc("", "print", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "print");
    }
    if ( xl6StdlibFindFunc("", "println", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "print");
    }

    if ( xl6StdlibFindFunc("string", "len", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
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
        xl6StdlibMarkFuncUsed(used, "__xrt_string", "inStr");
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
    if ( xl6StdlibFindFunc("string", "formatFloat", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "takeCstr");
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
    if ( xl6StdlibFindFunc("time", "diffDays", &index) != NULL && used[index] ) {
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

    if ( xl6StdlibFindFunc("random", "u32", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_random", "rand32");
    }
    if ( xl6StdlibFindFunc("random", "u64", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_random", "rand64");
    }
    if ( xl6StdlibFindFunc("random", "range", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_random", "range");
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
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "readAll");
    }
    if ( xl6StdlibFindFunc("file", "writeText", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "writeAll");
    }
    if ( xl6StdlibFindFunc("file", "appendText", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_file", "append");
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

    if ( xl6StdlibFindFunc("json", "parse", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xrt_json", "parse");
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

    if ( xl6StdlibFindFunc("regex", "isMatch", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "regexIsMatch");
    }
    if ( xl6StdlibFindFunc("regex", "find", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__crt_string", "strlen");
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "regexFind");
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

    if ( xl6StdlibFindFunc("http.server", "create", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerCreate");
    }
    if ( xl6StdlibFindFunc("http.server", "start", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerStart");
    }
    if ( xl6StdlibFindFunc("http.server", "port", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerPort");
    }
    if ( xl6StdlibFindFunc("http.server", "stop", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerStop");
    }
    if ( xl6StdlibFindFunc("http.server", "destroy", &index) != NULL && used[index] ) {
        xl6StdlibMarkFuncUsed(used, "__xl_runtime", "httpServerDestroy");
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

    /* 释放原成员访问结构，保留节点位置用于诊断。 */
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

static int xl6StdlibRewriteRefs(XL6_AstNode* node, int* used, XL6_DiagList* diags)
{
    char ns[128];
    size_t index;
    size_t i;
    int ok;

    /* 全局标准库函数：print(...) -> __xl_std_print(...)。 */
    if ( node != NULL && node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME ) {
        XL6_AstNode* callee = node->children[0];
        const XL6_StdlibFunc* func = xl6StdlibFindFunc("", callee->text, &index);
        if ( func != NULL ) {
            used[index] = 1;
            xrtFree(callee->text);
            callee->text = xl6StdlibDupText(func->internalName);
            if ( callee->text == NULL ) {
                return 0;
            }
        }
    }

    if ( node == NULL ) {
        return 1;
    }

    /* 标准库命名空间：math.log -> __xl_std_math_log。 */
    if ( node->kind == XL6_AST_MEMBER && node->childCount > 0 && node->children[0] != NULL ) {
        if ( xl6StdlibReceiverPath(node->children[0], ns, sizeof(ns)) ) {
            const XL6_StdlibFunc* func = xl6StdlibFindFunc(ns, node->text, &index);
            if ( func != NULL ) {
                used[index] = 1;
                xl6StdlibRewriteMemberToName(node, func->internalName);
                return node->text != NULL;
            }
            if ( xl6StdlibNamespaceExists(ns) ) {
                if ( diags != NULL ) {
                    xl6DiagAdd(diags, node->line, node->column, "standard library namespace has no such function");
                }
                return 0;
            }
        }
    }

    ok = 1;
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6StdlibRewriteRefs(node->children[i], used, diags) ) {
            ok = 0;
        }
    }
    return ok;
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
    int used[XL6_ARRAY_COUNT(xl6StdlibFuncs)];
    XL6_StdlibTextBuilder builder;
    size_t i;
    int ok;

    if ( root == NULL ) {
        return 1;
    }

    memset(used, 0, sizeof(used));
    ok = xl6StdlibRewriteRefs(root, used, diags);
    if ( !ok ) {
        return 0;
    }
    xl6StdlibApplyDeps(used);

    /* 只为实际用到的标准库函数注入 extern 声明。 */
    xl6StdlibBuilderInit(&builder);
    for ( i = 0; i < XL6_ARRAY_COUNT(xl6StdlibFuncs); ++i ) {
        if ( used[i] ) {
            xl6StdlibAppendFuncDecl(&builder, &xl6StdlibFuncs[i]);
        }
    }
    ok = builder.ok && xl6StdlibMergeGeneratedDecls(root, builder.data, diags);
    xl6StdlibBuilderFree(&builder);
    return ok;
}
