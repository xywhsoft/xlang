/* 可读单位：仅组合 CRT 算术/格式化与 XRT 错误、内存能力。 */
#ifndef XRT_MODULE_NUMBER_FORMAT
#define XRT_MODULE_NUMBER_FORMAT
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"
#include "include/units_native.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define XU_DOMAIN "xlang.extlib.units"
#define XU_MAX_BYTES 1e21
#define XU_MAX_SECONDS 1e12

static void xu_error(const char* message)
{
    xrtSetErrorInfo(XERR_VALUE, XU_DOMAIN, 0, message);
}

static bool xu_valid(double value, double limit)
{
    if (isfinite(value) && fabs(value) <= limit) return true;
    xu_error("value must be finite and within supported range");
    return false;
}

/* 整数显示沿用当前舍入模式，但不向用户呈现负零。 */
static double xu_display_whole(double value)
{
    double rounded = nearbyint(value);
    return rounded == 0.0 ? 0.0 : rounded;
}

/* 固定小数位使用 XRT 数字格式，避免 CRT 区域设置改变小数点。 */
static bool xu_fixed(char* text, size_t capacity, double value, int digits)
{
    xstrview format = digits == 0 ? XRT_STR_LITERAL(".0f") :
        digits == 1 ? XRT_STR_LITERAL(".1f") : XRT_STR_LITERAL(".2f");
    size_t length;
    return xrtNumFormatTo(value, format, text, capacity, &length);
}

/* 升档依据最终显示值，避免 fenv 舍入与 XRT 固定精度舍入产生分歧。 */
static bool xu_shown_reaches(double value, int digits,
    const char* threshold, bool* reaches)
{
    char text[64];
    const char* point;
    size_t whole;
    if (!xu_fixed(text, sizeof(text), value, digits)) return false;
    point = strchr(text, '.');
    whole = point != NULL ? (size_t)(point - text) : strlen(text);
    *reaches = whole > 4u ||
        (whole == 4u && memcmp(text, threshold, 4u) >= 0);
    return true;
}

static char* xu_format_bytes(double value, double scale, const char* const* names)
{
    char text[96];
    char number[64];
    double amount = fabs(value);
    int unit = 0;
    int length;
    xrtClearError();
    if (!xu_valid(value, XU_MAX_BYTES)) return NULL;
    /* 舍入后的显示值到达下一档时同步升档，避免输出 1024.00 KiB。 */
    while (unit < 6) {
        bool reaches;
        if (unit == 0) reaches = fabs(xu_display_whole(value)) >= scale;
        else if (amount >= scale) reaches = true;
        else if (amount < scale - 0.01) reaches = false;
        else if (!xu_shown_reaches(amount, 2,
            scale == 1024.0 ? "1024" : "1000", &reaches)) return NULL;
        if (!reaches) break;
        amount /= scale;
        ++unit;
    }
    if (!xu_fixed(number, sizeof(number),
        unit == 0 ? xu_display_whole(value) : amount, unit == 0 ? 0 : 2)) return NULL;
    length = snprintf(text, sizeof(text), "%s%s %s",
        unit != 0 && value < 0 ? "-" : "", number, names[unit]);
    if (length < 0 || (size_t)length >= sizeof(text)) {
        xu_error("formatted byte size exceeds output limit");
        return NULL;
    }
    return xrtStrDup(text);
}

char* xu_bytes(double value)
{
    static const char* const names[] =
        {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    return xu_format_bytes(value, 1024.0, names);
}

char* xu_bytes_decimal(double value)
{
    static const char* const names[] =
        {"B", "kB", "MB", "GB", "TB", "PB", "EB"};
    return xu_format_bytes(value, 1000.0, names);
}

/* 避开 TCC strtod 接受 Fortran d 指数造成的 "1d2h" 误读。 */
static bool xu_number(const char** cursor, double* output)
{
    const char* p = *cursor;
    double number = 0.0, fraction = 0.1;
    bool seen = false;
    while (*p >= '0' && *p <= '9') {
        number = number * 10.0 + (*p++ - '0');
        if (!isfinite(number) || number > XU_MAX_BYTES) return false;
        seen = true;
    }
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') {
            number += (*p++ - '0') * fraction;
            fraction /= 10.0;
            seen = true;
        }
    }
    *cursor = p;
    *output = number;
    return seen;
}

static const char* xu_spaces(const char* p)
{
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

static bool xu_input(const char* text, long long length, char copy[129])
{
    if (text == NULL || length < 0 || length > 128 ||
        memchr(text, '\0', (size_t)length) != NULL) {
        xu_error("input must be at most 128 bytes without embedded NUL");
        return false;
    }
    memcpy(copy, text, (size_t)length);
    copy[length] = '\0';
    return true;
}

double xu_parse_bytes(const char* text, long long length)
{
    static const char* const si[] = {"KB", "MB", "GB", "TB", "PB", "EB"};
    static const char* const iec[] = {"KIB", "MIB", "GIB", "TIB", "PIB", "EIB"};
    static const double si_factor[] = {1e3, 1e6, 1e9, 1e12, 1e15, 1e18};
    static const double iec_factor[] =
        {1024.0, 1048576.0, 1073741824.0, 1099511627776.0,
         1125899906842624.0, 1152921504606846976.0};
    const char* p;
    char input[129];
    char suffix[5] = {0};
    double number, factor = 1.0;
    int sign = 1, i;
    size_t suffix_length = 0u;
    xrtClearError();
    if (!xu_input(text, length, input)) return NAN;
    p = xu_spaces(input);
    if (*p == '+' || *p == '-') { if (*p == '-') sign = -1; ++p; }
    if (!xu_number(&p, &number)) goto bad;
    p = xu_spaces(p);
    while (*p != '\0' && *p != ' ' && *p != '\t') {
        unsigned char letter = (unsigned char)*p++;
        if (suffix_length >= sizeof(suffix) - 1u) goto bad;
        if (letter >= 'a' && letter <= 'z') letter -= 'a' - 'A';
        suffix[suffix_length++] = (char)letter;
    }
    if (*xu_spaces(p) != '\0') goto bad;
    if (suffix_length != 0u && strcmp(suffix, "B") != 0) {
        factor = 0.0;
        for (i = 0; i < 6; ++i) {
            if (strcmp(suffix, si[i]) == 0) factor = si_factor[i];
            if (strcmp(suffix, iec[i]) == 0) factor = iec_factor[i];
        }
        if (factor == 0.0) goto bad;
    }
    number *= factor * sign;
    if (!xu_valid(number, XU_MAX_BYTES)) return NAN;
    return number;
bad:
    xu_error("invalid byte size; use B, kB..EB or KiB..EiB");
    return NAN;
}

static char* xu_format_duration(double seconds, bool short_style)
{
    char text[96];
    unsigned long long rounded, days, hours, minutes, rest;
    int length;
    xrtClearError();
    if (!xu_valid(seconds, XU_MAX_SECONDS)) return NULL;
    rounded = (unsigned long long)(fabs(seconds) + 0.5);
    days = rounded / 86400u;
    hours = (rounded / 3600u) % 24u;
    minutes = (rounded / 60u) % 60u;
    rest = rounded % 60u;
    if (short_style) {
        if (days != 0u)
            length = snprintf(text, sizeof(text), "%s%llud%02llu:%02llu:%02llu",
                seconds < 0 ? "-" : "", days, hours, minutes, rest);
        else if (hours != 0u)
            length = snprintf(text, sizeof(text), "%s%llu:%02llu:%02llu",
                seconds < 0 ? "-" : "", hours, minutes, rest);
        else
            length = snprintf(text, sizeof(text), "%s%llu:%02llu",
                seconds < 0 && rounded != 0u ? "-" : "", minutes, rest);
    } else {
        char* out = text;
        size_t left = sizeof(text);
#define XU_PART(...) do { \
    int written = snprintf(out, left, __VA_ARGS__); \
    if (written < 0 || (size_t)written >= left) goto overflow; \
    out += written; left -= (size_t)written; \
} while (0)
        if (seconds < 0 && rounded != 0u) XU_PART("-");
        if (days != 0u) XU_PART("%llud", days);
        if (hours != 0u) XU_PART("%s%lluh", out == text || (out == text + 1 && text[0] == '-') ? "" : " ", hours);
        if (minutes != 0u) XU_PART("%s%llum", out == text || (out == text + 1 && text[0] == '-') ? "" : " ", minutes);
        if (rest != 0u || rounded == 0u) XU_PART("%s%llus", out == text || (out == text + 1 && text[0] == '-') ? "" : " ", rest);
        length = (int)(out - text);
#undef XU_PART
    }
    if (length < 0 || (size_t)length >= sizeof(text)) goto overflow;
    return xrtStrDup(text);
overflow:
    xu_error("formatted duration exceeds output limit");
    return NULL;
}

char* xu_duration(double seconds) { return xu_format_duration(seconds, false); }
char* xu_duration_short(double seconds) { return xu_format_duration(seconds, true); }

double xu_parse_duration(const char* text, long long length)
{
    const char* p;
    char input[129];
    double total = 0.0;
    int previous_rank = 5, sign = 1, any = 0;
    xrtClearError();
    if (!xu_input(text, length, input)) return NAN;
    p = xu_spaces(input);
    if (*p == '+' || *p == '-') { if (*p == '-') sign = -1; ++p; }
    while (*(p = xu_spaces(p)) != '\0') {
        double number, factor;
        int rank;
        if (!xu_number(&p, &number)) goto bad;
        p = xu_spaces(p);
        if (*p == 'd') { rank = 4; factor = 86400.0; }
        else if (*p == 'h') { rank = 3; factor = 3600.0; }
        else if (*p == 'm') { rank = 2; factor = 60.0; }
        else if (*p == 's') { rank = 1; factor = 1.0; }
        else if (*p == '\0' && any == 0) { rank = 0; factor = 1.0; }
        else goto bad;
        if (rank >= previous_rank) goto bad;
        previous_rank = rank;
        total += number * factor;
        if (!isfinite(total) || total > XU_MAX_SECONDS) goto bad;
        any = 1;
        if (*p == '\0') break;
        ++p;
    }
    if (any) return total * sign;
bad:
    xu_error("invalid duration; use ordered d, h, m, s components");
    return NAN;
}

char* xu_count(double value)
{
    static const char* const suffixes[] = {"", "K", "M", "B", "T"};
    static const double factors[] = {1.0, 1e3, 1e6, 1e9, 1e12};
    char text[96];
    char number[64];
    double magnitude;
    int unit = 0, length;
    xrtClearError();
    if (!xu_valid(value, XU_MAX_BYTES)) return NULL;
    magnitude = fabs(value);
    while (unit < 4) {
        bool reaches;
        if (unit == 0) reaches = fabs(xu_display_whole(value)) >= 1000.0;
        else if (magnitude / factors[unit] >= 1000.0) reaches = true;
        else if (magnitude / factors[unit] < 999.9) reaches = false;
        else if (!xu_shown_reaches(magnitude / factors[unit], 1,
            "1000", &reaches)) return NULL;
        if (!reaches) break;
        ++unit;
    }
    if (!xu_fixed(number, sizeof(number), unit == 0 ?
        xu_display_whole(value) : magnitude / factors[unit], unit == 0 ? 0 : 1)) return NULL;
    length = snprintf(text, sizeof(text), "%s%s%s",
        unit != 0 && value < 0 ? "-" : "", number, suffixes[unit]);
    if (length < 0 || (size_t)length >= sizeof(text)) {
        xu_error("formatted count exceeds output limit");
        return NULL;
    }
    return xrtStrDup(text);
}

int xu_is_pow2(int64_t value)
{
    xrtClearError();
    return value > 0 && (value & (value - 1)) == 0;
}
