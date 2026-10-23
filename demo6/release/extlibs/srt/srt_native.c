/* SRT/WebVTT 文本边界在此处理；值对象与动态缓冲均使用 XRT。 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "include/srt_native.h"

#define XSRT_DOMAIN "xlang.extlib.srt"

typedef struct { const char *data; size_t size; } XsrtLine;

static void xsrt_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSRT_DOMAIN, 0, message);
}

static int xsrt_equal(XsrtLine line, const char *text) {
    size_t size = strlen(text);
    return line.size == size && memcmp(line.data, text, size) == 0;
}

static int xsrt_prefix(XsrtLine line, const char *text) {
    size_t size = strlen(text);
    return line.size >= size && memcmp(line.data, text, size) == 0;
}

static int xsrt_vtt_control(XsrtLine line) {
    return xsrt_equal(line, "NOTE") || xsrt_prefix(line, "NOTE ") ||
           xsrt_prefix(line, "NOTE\t") || xsrt_equal(line, "STYLE") ||
           xsrt_equal(line, "REGION");
}

static int xsrt_next(const char **cursor, const char *limit, XsrtLine *line) {
    const char *end;
    if (*cursor == NULL || *cursor == limit) return 0;
    end = (const char *)memchr(*cursor, '\n', (size_t)(limit - *cursor));
    if (end == NULL) end = limit;
    line->data = *cursor;
    line->size = (size_t)(end - *cursor);
    *cursor = end < limit ? end + 1 : NULL;
    if (line->size && line->data[line->size - 1] == '\r') line->size--;
    return 1;
}

static XsrtLine xsrt_trim(XsrtLine line) {
    while (line.size && (line.data[0] == ' ' || line.data[0] == '\t')) {
        line.data++; line.size--;
    }
    while (line.size && (line.data[line.size - 1] == ' ' ||
                         line.data[line.size - 1] == '\t')) line.size--;
    return line;
}

static size_t xsrt_arrow(XsrtLine line) {
    size_t i;
    for (i = 0; i + 3 <= line.size; ++i)
        if (memcmp(line.data + i, "-->", 3) == 0) return i;
    return SIZE_MAX;
}

static xvalue *xsrt_string(XsrtLine line) {
    return xrtValueString((xstrview){line.data ? line.data : "", line.size});
}

static int xsrt_two(XsrtLine *line, int *value) {
    unsigned char a, b;
    if (line->size < 2) return 0;
    a = (unsigned char)line->data[0];
    b = (unsigned char)line->data[1];
    if (a < '0' || a > '9' || b < '0' || b > '9') return 0;
    *value = (a - '0') * 10 + (b - '0');
    line->data += 2; line->size -= 2;
    return 1;
}

/* 毫秒固定三位；不接受尾随垃圾、负数或 int64 溢出。 */
static int xsrt_stamp(XsrtLine line, int vtt, int64_t *out) {
    int64_t first = 0, hours = 0;
    int minutes, seconds, millis, second;
    size_t digits = 0;
    while (line.size && line.data[0] >= '0' && line.data[0] <= '9') {
        int digit = line.data[0] - '0';
        if (first > (INT64_MAX / 3600000 - digit) / 10) return 0;
        first = first * 10 + digit;
        line.data++; line.size--; digits++;
    }
    if (digits < 2 || !line.size || line.data[0] != ':') return 0;
    line.data++; line.size--;
    if (!xsrt_two(&line, &second)) return 0;
    if (vtt && line.size && line.data[0] == '.') {
        if (first > 59) return 0;
        minutes = (int)first;
        seconds = second;
    } else {
        if (!line.size || line.data[0] != ':') return 0;
        line.data++; line.size--;
        if (!xsrt_two(&line, &seconds)) return 0;
        hours = first;
        minutes = second;
    }
    if (minutes > 59 || seconds > 59 || !line.size ||
        line.data[0] != (vtt ? '.' : ',')) return 0;
    line.data++; line.size--;
    if (line.size != 3) return 0;
    if (!xsrt_two(&line, &millis)) return 0;
    if (line.data[0] < '0' || line.data[0] > '9') return 0;
    millis = millis * 10 + line.data[0] - '0';
    if (hours > (INT64_MAX - (int64_t)minutes * 60000 - seconds * 1000 - millis) / 3600000) return 0;
    *out = hours * 3600000 + (int64_t)minutes * 60000 + seconds * 1000 + millis;
    return 1;
}

static int xsrt_range(XsrtLine line, int vtt, int64_t *start, int64_t *end,
                      XsrtLine *settings) {
    size_t arrow = xsrt_arrow(line);
    XsrtLine left, right;
    size_t i;
    if (arrow == SIZE_MAX) return 0;
    /* WebVTT 时间轴的箭头两侧必须各有空格或 tab；SRT 保留原有宽容解析。 */
    if (vtt && (arrow == 0 || arrow + 3 == line.size ||
        (line.data[arrow - 1] != ' ' && line.data[arrow - 1] != '\t') ||
        (line.data[arrow + 3] != ' ' && line.data[arrow + 3] != '\t'))) return 0;
    left = xsrt_trim((XsrtLine){line.data, arrow});
    right = xsrt_trim((XsrtLine){line.data + arrow + 3, line.size - arrow - 3});
    settings->data = right.data + right.size;
    settings->size = 0;
    if (vtt) {
        for (i = 0; i < right.size; ++i)
            if (right.data[i] == ' ' || right.data[i] == '\t') break;
        if (i < right.size) {
            *settings = xsrt_trim((XsrtLine){right.data + i, right.size - i});
            right.size = i;
            if (xsrt_arrow(*settings) != SIZE_MAX) return 0;
        }
    }
    return xsrt_stamp(left, vtt, start) && xsrt_stamp(right, vtt, end) && *end >= *start;
}

static int xsrt_put(xvalue *object, const char *key, xvalue *item) {
    xstrview name = {key, strlen(key)};
    return item != NULL && xrtValueObjectSetNew(object, name, item);
}

static xvalue *xsrt_parse_cues(const char *text, size_t size, int vtt) {
    const char *cursor = text;
    const char *limit;
    XsrtLine line = {0};
    xvalue *result;
    xvalue *seenIds = NULL;
    int64_t nextIndex = 1;
    int64_t previousStart = -1;
    size_t i;
    xrtClearError();
    if (text == NULL) { xsrt_error("subtitle text is null"); return xrtValueNull(); }
    if (size == SIZE_MAX || memchr(text, 0, size) != NULL) {
        xsrt_error("subtitle contains an embedded NUL or invalid input");
        return xrtValueNull();
    }
    if (!xrtUtf8Valid((xstrview){text, size}, NULL)) {
        xsrt_error("subtitle text must be valid UTF-8");
        return xrtValueNull();
    }
    for (i = 0; i < size; ++i) {
        if (text[i] == '\r' && (i + 1 == size || text[i + 1] != '\n')) {
            xsrt_error("subtitle lines must use LF or CRLF");
            return xrtValueNull();
        }
    }
    limit = text + size;
    result = xrtValueArray();
    if (result == NULL) return NULL;
    if (vtt) {
        int headerEnded = 0;
        if (!xsrt_next(&cursor, limit, &line)) goto invalid;
        if (line.size >= 3 && memcmp(line.data, "\xef\xbb\xbf", 3) == 0) {
            line.data += 3; line.size -= 3;
        }
        if (!xsrt_prefix(line, "WEBVTT") ||
            (line.size > 6 && line.data[6] != ' ' && line.data[6] != '\t')) goto invalid;
        while (xsrt_next(&cursor, limit, &line)) {
            if (!line.size) { headerEnded = 1; break; }
        }
        if (!headerEnded) goto invalid;
    } else if (size >= 3 && memcmp(cursor, "\xef\xbb\xbf", 3) == 0) cursor += 3;

    while (xsrt_next(&cursor, limit, &line)) {
        XsrtLine id = {0}, settings = {0};
        int64_t start, end, index;
        xvalue *cue = NULL, *lines = NULL;
        size_t k;
        if (!line.size) continue;
        if (vtt && xsrt_vtt_control(line)) {
            if ((xsrt_equal(line, "STYLE") || xsrt_equal(line, "REGION")) &&
                xrtValueCount(result) > 0) goto invalid;
            if (xsrt_arrow(line) != SIZE_MAX) goto invalid;
            while (xsrt_next(&cursor, limit, &line) && line.size)
                if (xsrt_arrow(line) != SIZE_MAX) goto invalid;
            continue;
        }
        if (xsrt_arrow(line) == SIZE_MAX) {
            id = line;
            if (!xsrt_next(&cursor, limit, &line)) goto invalid;
        }
        if (!xsrt_range(line, vtt, &start, &end, &settings)) goto invalid;
        if (vtt && start < previousStart) goto invalid;
        index = nextIndex++;
        if (!vtt) {
            if (!id.size) goto invalid;
            index = 0;
            for (k = 0; k < id.size; ++k) {
                unsigned char ch = (unsigned char)id.data[k];
                if (ch < '0' || ch > '9' || index > (INT64_MAX - (ch - '0')) / 10) goto invalid;
                index = index * 10 + (ch - '0');
            }
            if (!index) goto invalid;
        }
        cue = xrtValueObject();
        lines = xrtValueArray();
        if (cue == NULL || lines == NULL) goto oom;
        if (!xsrt_put(cue, "index", xrtValueInt(index)) ||
            !xsrt_put(cue, "startMs", xrtValueInt(start)) ||
            !xsrt_put(cue, "endMs", xrtValueInt(end))) goto oom;
        if (vtt && id.size && !xsrt_put(cue, "id", xsrt_string(id))) goto oom;
        if (vtt && settings.size && !xsrt_put(cue, "settings", xsrt_string(settings))) goto oom;
        if (vtt && id.size) {
            xstrview key = {id.data, id.size};
            if (seenIds == NULL) {
                seenIds = xrtValueObject();
                if (seenIds == NULL) goto oom;
            }
            if (xrtValueObjectGet(seenIds, key) != NULL) goto invalid_cue;
            if (!xrtValueObjectSetNew(seenIds, key, xrtValueBool(true))) goto oom;
        }
        while (xsrt_next(&cursor, limit, &line) && line.size) {
            if (vtt && xsrt_arrow(line) != SIZE_MAX) goto invalid_cue;
            if (!xrtValueArrayAppendNew(lines, xsrt_string(line))) goto oom;
        }
        if (!xsrt_put(cue, "lines", lines)) { lines = NULL; goto oom; }
        lines = NULL;
        if (!xrtValueArrayAppendNew(result, cue)) { cue = NULL; goto oom; }
        if (vtt) previousStart = start;
        continue;
invalid_cue:
        xrtValueRelease(lines);
        xrtValueRelease(cue);
        goto invalid;
oom:
        xrtValueRelease(lines);
        xrtValueRelease(cue);
        xrtValueRelease(seenIds);
        xrtValueRelease(result);
        return NULL;
    }
    xrtValueRelease(seenIds);
    return result;
invalid:
    xrtValueRelease(seenIds);
    xrtValueRelease(result);
    xsrt_error("invalid subtitle cue or timestamp");
    return xrtValueNull();
}

xvalue *xsrt_parse(const char *text) {
    return xsrt_parse_cues(text, text != NULL ? strlen(text) : 0, 0);
}
xvalue *xsrt_parse_vtt(const char *text) {
    return xsrt_parse_cues(text, text != NULL ? strlen(text) : 0, 1);
}

xvalue *xsrt_parse_n(const char *text, size_t size) {
    return xsrt_parse_cues(text, size, 0);
}

xvalue *xsrt_parse_vtt_n(const char *text, size_t size) {
    return xsrt_parse_cues(text, size, 1);
}

static xvalue *xsrt_field(xvalue *object, const char *name) {
    int64 i, count;
    size_t size = strlen(name);
    if (object == NULL || xrtValueType(object) != XVALUE_OBJECT) return NULL;
    count = xrtValueCount(object);
    for (i = 0; i < count; ++i) {
        xstrview key;
        xvalue *value = xrtValueObjectAt(object, i, &key);
        if (key.Size == size && memcmp(key.Data, name, size) == 0) return value;
    }
    return NULL;
}

static int xsrt_integer(xvalue *cue, const char *name, int64_t *out) {
    xvalue *value = xsrt_field(cue, name);
    return value != NULL && xrtValueType(value) == XVALUE_INT &&
           xrtValueGetInt(value, out) && *out >= 0;
}

static int xsrt_append(xbuffer *buffer, const char *text, size_t size) {
    return xrtBufferAppend(buffer, (xbytesview){(const uint8 *)text, size});
}

static int xsrt_literal(xbuffer *buffer, const char *text) {
    return xsrt_append(buffer, text, strlen(text));
}

static int xsrt_format(xbuffer *buffer, int64_t ms, char separator) {
    char stamp[64];
    int n;
    if (ms < 0) return 0;
    n = snprintf(stamp, sizeof(stamp), "%02lld:%02lld:%02lld%c%03lld",
        (long long)(ms / 3600000), (long long)((ms / 60000) % 60),
        (long long)((ms / 1000) % 60), separator, (long long)(ms % 1000));
    return n > 0 && (size_t)n < sizeof(stamp) && xsrt_append(buffer, stamp, (size_t)n);
}

static int xsrt_optional(xbuffer *buffer, xvalue *cue, const char *name, const char *suffix) {
    xvalue *value = xsrt_field(cue, name);
    xstrview view;
    size_t i;
    if (value == NULL) return 1;
    if (xrtValueType(value) != XVALUE_STRING || !xrtValueGetString(value, &view)) return 0;
    if (!view.Size || !xrtUtf8Valid(view, NULL)) return 0;
    if (strcmp(name, "id") == 0 && xsrt_vtt_control((XsrtLine){view.Data, view.Size})) return 0;
    for (i = 0; i < view.Size; ++i)
        if (view.Data[i] == '\n' || view.Data[i] == '\r' || view.Data[i] == '\0' ||
            (i + 3 <= view.Size && memcmp(view.Data + i, "-->", 3) == 0)) return 0;
    return xsrt_append(buffer, view.Data, view.Size) && xsrt_literal(buffer, suffix);
}

/* 返回独立 XRT 缓冲；C 侧 xrtFree，XL 包装调用 takeCstr。 */
static char *xsrt_build_cues(xvalue *cues, int vtt) {
    xbuffer buffer;
    xvalue *seenIds = NULL;
    int64_t previousStart = -1;
    int64 count, i;
    xrtClearError();
    if (cues == NULL || xrtValueType(cues) != XVALUE_ARRAY) goto invalid;
    if (!xrtBufferInit(&buffer)) return NULL;
    if (vtt && !xsrt_literal(&buffer, "WEBVTT\n\n")) goto fail;
    count = xrtValueCount(cues);
    for (i = 0; i < count; ++i) {
        xvalue *cue = xrtValueArrayAt(cues, i);
        xvalue *lines = xsrt_field(cue, "lines");
        int64_t start, end;
        int64 j, lineCount;
        char index[32];
        int n;
        if (lines == NULL || xrtValueType(lines) != XVALUE_ARRAY ||
            !xsrt_integer(cue, "startMs", &start) ||
            !xsrt_integer(cue, "endMs", &end) || end < start) goto bad_buffer;
        if (vtt && start < previousStart) goto bad_buffer;
        if (vtt) {
            xvalue *idValue;
            if (!xsrt_optional(&buffer, cue, "id", "\n")) goto bad_buffer;
            idValue = xsrt_field(cue, "id");
            if (idValue != NULL) {
                xstrview key;
                if (!xrtValueGetString(idValue, &key)) goto bad_buffer;
                if (seenIds == NULL) {
                    seenIds = xrtValueObject();
                    if (seenIds == NULL) goto fail;
                }
                if (xrtValueObjectGet(seenIds, key) != NULL) goto bad_buffer;
                if (!xrtValueObjectSetNew(seenIds, key, xrtValueBool(true))) goto fail;
            }
        } else {
            n = snprintf(index, sizeof(index), "%lld\n", (long long)(i + 1));
            if (n <= 0 || !xsrt_append(&buffer, index, (size_t)n)) goto fail;
        }
        if (!xsrt_format(&buffer, start, vtt ? '.' : ',') ||
            !xsrt_literal(&buffer, " --> ") ||
            !xsrt_format(&buffer, end, vtt ? '.' : ',')) goto fail;
        if (vtt && xsrt_field(cue, "settings")) {
            if (!xsrt_literal(&buffer, " ") ||
                !xsrt_optional(&buffer, cue, "settings", "")) goto bad_buffer;
        }
        if (!xsrt_literal(&buffer, "\n")) goto fail;
        lineCount = xrtValueCount(lines);
        for (j = 0; j < lineCount; ++j) {
            xvalue *item = xrtValueArrayAt(lines, j);
            xstrview view;
            size_t k;
            if (item == NULL || xrtValueType(item) != XVALUE_STRING ||
                !xrtValueGetString(item, &view)) goto bad_buffer;
            if (!view.Size || !xrtUtf8Valid(view, NULL)) goto bad_buffer;
            if (vtt && xsrt_arrow((XsrtLine){view.Data, view.Size}) != SIZE_MAX)
                goto bad_buffer;
            for (k = 0; k < view.Size; ++k)
                if (view.Data[k] == '\n' || view.Data[k] == '\r' || view.Data[k] == '\0') goto bad_buffer;
            if (!xsrt_append(&buffer, view.Data, view.Size) ||
                !xsrt_literal(&buffer, "\n")) goto fail;
        }
        if (!xsrt_literal(&buffer, "\n")) goto fail;
        if (vtt) previousStart = start;
    }
    if (!xrtBufferAppendByte(&buffer, 0)) goto fail;
    xrtValueRelease(seenIds);
    return (char *)xrtBufferTake(&buffer, NULL, NULL);
bad_buffer:
    /* 可选字段写入失败也会到达这里，不得覆盖分配错误。 */
    if (xrtGetError() == NULL || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
        xsrt_error("invalid cue: expected ordered milliseconds and string lines");
fail:
    xrtValueRelease(seenIds);
    xrtBufferUnit(&buffer);
    return NULL;
invalid:
    xsrt_error("cues must be an array");
    return NULL;
}

void *xsrt_build(xvalue *cues) { return xsrt_build_cues(cues, 0); }
void *xsrt_build_vtt(xvalue *cues) { return xsrt_build_cues(cues, 1); }

void *xsrt_format_time(int64_t ms) {
    xbuffer buffer;
    xrtClearError();
    if (ms < 0) { xsrt_error("timestamp must be non-negative"); return NULL; }
    if (!xrtBufferInit(&buffer)) return NULL;
    if (!xsrt_format(&buffer, ms, ',') || !xrtBufferAppendByte(&buffer, 0)) {
        xrtBufferUnit(&buffer);
        return NULL;
    }
    return (char *)xrtBufferTake(&buffer, NULL, NULL);
}

int64_t xsrt_parse_time(const char *text) {
    XsrtLine line;
    int64_t result;
    xrtClearError();
    if (text != NULL) {
        line = (XsrtLine){text, strlen(text)};
        if (xsrt_stamp(line, 0, &result) || xsrt_stamp(line, 1, &result)) return result;
    }
    xsrt_error("invalid timestamp");
    return -1;
}

int64_t xsrt_parse_time_n(const char *text, size_t size) {
    XsrtLine line = {text, size};
    int64_t result;
    xrtClearError();
    if (text != NULL && size != SIZE_MAX && (xsrt_stamp(line, 0, &result) ||
                         xsrt_stamp(line, 1, &result))) return result;
    xsrt_error("invalid timestamp");
    return -1;
}
