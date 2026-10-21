/* 五字段本地时间 cron；解析无共享状态，时间分解复用 XRT。 */
#include "include/crontab_native.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#ifndef XRT_MODULE_TIME_LOCAL
#define XRT_MODULE_TIME_LOCAL
#endif
#include "xrt.h"

#define XCRON_MAX_TEXT 1024
#define XCRON_SEARCH_MINUTES (INT64_C(8) * 366 * 24 * 60)

typedef struct {
    uint64_t mask;
    bool leading_star;
    const char *text;
    size_t size;
} XcronField;

typedef struct {
    XcronField field[5];
} XcronExpr;

static const int xcron_min[5] = {0, 0, 1, 1, 0};
static const int xcron_max[5] = {59, 23, 31, 12, 7};

static int xcron_number(const char **cursor, const char *end, int maximum, int *out) {
    int value = 0, any = 0;
    while (*cursor < end && **cursor >= '0' && **cursor <= '9') {
        int digit = **cursor - '0';
        if (digit > maximum || value > (maximum - digit) / 10) return 0;
        value = value * 10 + digit;
        ++*cursor;
        any = 1;
    }
    if (!any) return 0;
    *out = value;
    return 1;
}

static int xcron_field(XcronField *field, int index) {
    const char *cursor = field->text;
    const char *end = cursor + field->size;
    int minimum = xcron_min[index], maximum = xcron_max[index];
    if (cursor == end) return 0;
    field->mask = 0;
    field->leading_star = *cursor == '*';
    while (cursor < end) {
        int lo, hi, step = 1, value;
        if (*cursor == '*') {
            lo = minimum;
            hi = maximum;
            ++cursor;
        } else {
            if (!xcron_number(&cursor, end, maximum, &lo) || lo < minimum) return 0;
            hi = lo;
            if (cursor < end && *cursor == '-') {
                ++cursor;
                if (!xcron_number(&cursor, end, maximum, &hi) || hi < lo) return 0;
            } else if (cursor < end && *cursor == '/') {
                hi = maximum;  /* N/S 从 N 起每隔 S 个值。 */
            }
        }
        if (cursor < end && *cursor == '/') {
            ++cursor;
            if (!xcron_number(&cursor, end, INT_MAX, &step) || step == 0) return 0;
        }
        value = lo;
        for (;;) {
            int bit = index == 4 && value == 7 ? 0 : value;
            field->mask |= UINT64_C(1) << bit;
            if (step > hi - value) break;
            value += step;
        }
        if (cursor == end) break;
        if (*cursor != ',') return 0;
        ++cursor;
        if (cursor == end) return 0;
    }
    return field->mask != 0;
}

static int xcron_parse(const char *text, size_t size, XcronExpr *out) {
    size_t position = 0;
    int index;
    if (text == NULL || out == NULL || size == 0 || size > XCRON_MAX_TEXT) return 0;
    for (index = 0; index < 5; ++index) {
        size_t start;
        while (position < size && (text[position] == ' ' || text[position] == '\t')) ++position;
        start = position;
        while (position < size && text[position] != ' ' && text[position] != '\t' && text[position] != '\0') ++position;
        if (start == position || (position < size && text[position] == '\0')) return 0;
        out->field[index].text = text + start;
        out->field[index].size = position - start;
        if (!xcron_field(&out->field[index], index)) return 0;
    }
    while (position < size && (text[position] == ' ' || text[position] == '\t')) ++position;
    return position == size;
}

static int xcron_match(const XcronExpr *expr, const xdatetime *date) {
    int day = (expr->field[2].mask & (UINT64_C(1) << date->Day)) != 0;
    int weekday = (expr->field[4].mask & (UINT64_C(1) << date->Weekday)) != 0;
    int dateMatch;
    dateMatch = (expr->field[2].leading_star || expr->field[4].leading_star) ?
        day && weekday : day || weekday;
    return dateMatch &&
        (expr->field[0].mask & (UINT64_C(1) << date->Minute)) != 0 &&
        (expr->field[1].mask & (UINT64_C(1) << date->Hour)) != 0 &&
        (expr->field[3].mask & (UINT64_C(1) << date->Month)) != 0;
}

static int xcron_local(int64_t seconds, xdatetime *date) {
    xtime time;
    if (!xrtTimeFromUnix(seconds, &time) || !xrtTimeLocal(time, date)) {
        xrtClearError();
        return 0;
    }
    return 1;
}

/* 仅当月内日不以 * 开头、星期以 * 开头时，可用月内日提前判定不可达。 */
static bool xcron_calendar_possible(const XcronExpr *expr) {
    static const unsigned char maxDay[13] =
        {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int month, day;
    if (!expr->field[4].leading_star || expr->field[2].leading_star) return true;
    for (month = 1; month <= 12; ++month) {
        if ((expr->field[3].mask & (UINT64_C(1) << month)) == 0) continue;
        for (day = 1; day <= maxDay[month]; ++day) {
            if ((expr->field[2].mask & (UINT64_C(1) << day)) != 0) return true;
        }
    }
    return false;
}

/* ------------ 公开 API ------------ */

bool xcron_is_valid(const char *text, size_t size) {
    XcronExpr expr;
    xrtClearError();
    return xcron_parse(text, size, &expr) != 0;
}

bool xcron_matches(const char *text, size_t size, int64_t timestamp) {
    XcronExpr expr;
    xdatetime date;
    xrtClearError();
    return xcron_parse(text, size, &expr) && xcron_local(timestamp, &date) &&
        xcron_match(&expr, &date);
}

int64_t xcron_next(const char *text, size_t size, int64_t after) {
    XcronExpr expr;
    int64_t candidate, delta, iteration, previous;
    int previousOffset;
    xdatetime date;
    xrtClearError();
    if (!xcron_parse(text, size, &expr) || !xcron_calendar_possible(&expr)) return -1;
    if (!xcron_local(after, &date)) return -1;
    /* 历史时区可能有秒级 UTC 偏移，不能用 Unix 秒数对齐本地分钟。 */
    delta = 60 - date.Second;
    if (after > INT64_MAX - delta) return -1;
    previous = after;
    previousOffset = date.Offset;
    candidate = after + delta;
    for (iteration = 0; iteration < XCRON_SEARCH_MINUTES; ++iteration) {
        if (!xcron_local(candidate, &date)) return -1;
        if (date.Offset != previousOffset) {
            int64_t probe;
            /* 时差切换可让新的本地整分钟落在原步长之前。 */
            for (probe = previous + 1; probe < candidate; ++probe) {
                xdatetime crossed;
                if (!xcron_local(probe, &crossed)) return -1;
                if (crossed.Second == 0 && xcron_match(&expr, &crossed))
                    return probe;
            }
        }
        if (date.Second == 0 && xcron_match(&expr, &date)) return candidate;
        previous = candidate;
        previousOffset = date.Offset;
        delta = 60 - date.Second;
        if (candidate > INT64_MAX - delta) break;
        candidate += delta;
    }
    return -1;
}

void *xcron_describe(const char *text, size_t size) {
    XcronExpr expr;
    static const char *labels[5] = {"minute", "hour", "day", "month", "weekday"};
    char *result;
    size_t capacity, used = 0;
    int index;
    xrtClearError();
    if (!xcron_parse(text, size, &expr)) {
        result = xrtMalloc(8);
        if (result != NULL) memcpy(result, "invalid", 8);
        return result;
    }
    /* 步长字段虽以 * 开头，却不是“每分钟”。 */
    if (expr.field[0].leading_star && expr.field[0].size == 1 &&
        expr.field[1].leading_star && expr.field[1].size == 1 &&
        expr.field[2].leading_star && expr.field[2].size == 1 &&
        expr.field[3].leading_star && expr.field[3].size == 1 &&
        expr.field[4].leading_star && expr.field[4].size == 1) {
        result = xrtMalloc(13);
        if (result != NULL) memcpy(result, "every minute", 13);
        return result;
    }
    capacity = size + 64;
    result = xrtMalloc(capacity);
    if (result == NULL) return NULL;
    for (index = 0; index < 5; ++index) {
        int written = snprintf(result + used, capacity - used, "%s%s=%.*s",
            index == 0 ? "" : "; ", labels[index], (int)expr.field[index].size,
            expr.field[index].text);
        if (written < 0 || (size_t)written >= capacity - used) {
            xrtFree(result);
            return NULL;
        }
        used += (size_t)written;
    }
    return result;
}
