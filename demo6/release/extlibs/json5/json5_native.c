/* JSON5 事件流转 XRT 动态值；词法和语法由 libpdjson5 负责。 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "vendor/pdjson5.h"
#include "json5_native.h"

#define XJSON5_DOMAIN "xlang.extlib.json5"
#define XJSON5_MAX_BYTES (64u * 1024u * 1024u)
#define XJSON5_MAX_DEPTH 64u
#define XJSON5_MAX_ITEMS 1000000u

static void xjson5_error(cstr message) {
    xrtSetErrorInfo(XERR_VALUE, XJSON5_DOMAIN, 0, message);
}

static void xjson5_limit_error(cstr message) {
    xrtSetErrorInfo(XERR_RANGE, XJSON5_DOMAIN, 0, message);
}

static void xjson5_parse_error(const pdjson_stream* parser) {
    char message[256];
    cstr detail = pdjson_get_error(parser);
    snprintf(message, sizeof(message), "JSON5 line %llu, column %llu: %s",
        (unsigned long long)pdjson_get_line(parser),
        (unsigned long long)pdjson_get_column(parser),
        detail ? detail : "invalid input");
    xrtSetErrorInfo(pdjson_get_error_subtype(parser) == PDJSON_ERROR_MEMORY ?
        XERR_MEMORY : XERR_VALUE, XJSON5_DOMAIN, 0, message);
}

static void* xjson5_malloc(size_t size, void* user) {
    (void)user;
    return xrtMalloc(size);
}

static void* xjson5_realloc(void* data, size_t size, void* user) {
    (void)user;
    return xrtRealloc(data, size);
}

static void xjson5_free(void* data, size_t size, void* user) {
    (void)size;
    (void)user;
    xrtFree(data);
}

static xvalue* xjson5_number(xstrview raw) {
    int64 integer;
    double number;
    size_t start = raw.Size > 0 && (raw.Data[0] == '+' || raw.Data[0] == '-') ? 1u : 0u;
    bool hex = raw.Size >= start + 2u && raw.Data[start] == '0' &&
        (raw.Data[start + 1u] == 'x' || raw.Data[start + 1u] == 'X');
    /* e/E 在十六进制中是数字，只有十进制字面量才把它们当指数标记。 */
    bool whole = raw.Size > start && memchr(raw.Data, '.', raw.Size) == NULL &&
        (hex || (memchr(raw.Data, 'e', raw.Size) == NULL &&
            memchr(raw.Data, 'E', raw.Size) == NULL)) &&
        !((raw.Data[start] >= 'A' && raw.Data[start] <= 'Z') ||
          (raw.Data[start] >= 'a' && raw.Data[start] <= 'z'));

    if (whole && xrtIntParse(raw, 0u, XNUMBER_PARSE_PREFIX, &integer)) {
        if (integer == 0 && raw.Data[0] == '-') return xrtValueFloat(-0.0);
        return xrtValueInt(integer);
    }
    if (hex) {
        size_t i;
        number = 0.0;
        for (i = start + 2u; i < raw.Size; ++i) {
            unsigned char c = (unsigned char)raw.Data[i];
            unsigned digit = c >= '0' && c <= '9' ? (unsigned)(c - '0') :
                c >= 'a' && c <= 'f' ? (unsigned)(c - 'a') + 10u :
                (unsigned)(c - 'A') + 10u;
            number = number * 16.0 + (double)digit;
        }
        /* 只有显式 Infinity 才能表示无穷大；有限字面量溢出必须报错。 */
        if (!isfinite(number)) goto unrepresentable;
        return xrtValueFloat(start && raw.Data[0] == '-' ? -number : number);
    }
    if (xrtNumParse(raw, XNUMBER_PARSE_SPECIAL, &number)) {
        if (!isfinite(number) && raw.Data[start] != 'I' &&
            raw.Data[start] != 'N') goto unrepresentable;
        xrtClearError();
        return xrtValueFloat(number);
    }
unrepresentable:
    xjson5_error("JSON5 number is not representable as int64 or float64");
    return NULL;
}

static xvalue* xjson5_value(pdjson_stream* parser, enum pdjson_type type,
    size_t depth, size_t* items) {
    xvalue* result;
    size_t size;
    cstr text;

    if (depth > XJSON5_MAX_DEPTH) {
        xjson5_limit_error("JSON5 nesting exceeds 64 levels");
        return NULL;
    }
    switch (type) {
        case PDJSON_NULL: return xrtValueRetain(xrtValueNull());
        case PDJSON_TRUE: return xrtValueRetain(xrtValueBool(true));
        case PDJSON_FALSE: return xrtValueRetain(xrtValueBool(false));
        case PDJSON_STRING:
            text = pdjson_get_value(parser, &size);
            return text && size > 0 ? xrtValueString((xstrview){ text, size - 1u }) : NULL;
        case PDJSON_NUMBER:
            text = pdjson_get_value(parser, &size);
            return text && size > 0 ? xjson5_number((xstrview){ text, size - 1u }) : NULL;
        case PDJSON_ARRAY:
        case PDJSON_OBJECT: {
            bool object = type == PDJSON_OBJECT;
            bool failed = false;
            enum pdjson_type next;
            result = object ? xrtValueObject() : xrtValueArray();
            if (!result) return NULL;
            while ((next = pdjson_next(parser)) != (object ? PDJSON_OBJECT_END : PDJSON_ARRAY_END)) {
                xvalue* child;
                str name = NULL;
                xstrview key;
                if (next == PDJSON_ERROR) { xjson5_parse_error(parser); failed = true; break; }
                if (next == PDJSON_DONE) {
                    xjson5_error("JSON5 container ends early");
                    failed = true;
                    break;
                }
                if (++*items > XJSON5_MAX_ITEMS) {
                    xjson5_limit_error("JSON5 container exceeds one million items");
                    failed = true;
                    break;
                }
                if (object) {
                    if (next != PDJSON_NAME) { xjson5_error("JSON5 object member name is missing"); failed = true; break; }
                    text = pdjson_get_name(parser, &size);
                    if (!text || size == 0) { xjson5_error("JSON5 object member name is invalid"); failed = true; break; }
                    name = (str)xrtMalloc(size);
                    if (!name) { failed = true; break; }
                    memcpy(name, text, size);
                    key = (xstrview){ name, size - 1u };
                    if (xrtValueObjectHas(result, key)) {
                        xrtFree(name);
                        xjson5_error("duplicate JSON5 object key");
                        failed = true;
                        break;
                    }
                    next = pdjson_next(parser);
                }
                child = xjson5_value(parser, next, depth + 1u, items);
                if (!child) { xrtFree(name); failed = true; break; }
                if (object ? !xrtValueObjectSetTake(result, key, &child) :
                    !xrtValueArrayAppendTake(result, &child)) {
                    xrtValueRelease(child);
                    xrtFree(name);
                    failed = true;
                    break;
                }
                xrtFree(name);
            }
            if (!failed && next == (object ? PDJSON_OBJECT_END : PDJSON_ARRAY_END)) return result;
            xrtValueRelease(result);
            return NULL;
        }
        case PDJSON_ERROR: xjson5_parse_error(parser); return NULL;
        default: xjson5_error("unexpected JSON5 token"); return NULL;
    }
}

xvalue* xjson5_parse_len(const char* text, size_t size) {
    static const pdjson_allocator allocator = {
        xjson5_malloc, xjson5_realloc, xjson5_free
    };
    pdjson_stream parser;
    xvalue* result;
    enum pdjson_type next;
    size_t items = 0u;

    xrtClearError();
    if (!text || size == 0u) {
        xjson5_error("JSON5 input is empty");
        return xrtValueRetain(xrtValueNull());
    }
    if (size > XJSON5_MAX_BYTES) {
        xjson5_limit_error("JSON5 input exceeds 64 MiB");
        return xrtValueRetain(xrtValueNull());
    }
    /* 注释也属于源文本；解析器跳过注释前先校验整份 UTF-8。 */
    if (!xrtUtf8Valid((xstrview){text, size}, NULL)) {
        xjson5_error("JSON5 input is not valid UTF-8");
        return xrtValueRetain(xrtValueNull());
    }
    pdjson_open_buffer(&parser, text, size);
    pdjson_set_allocator(&parser, &allocator, NULL);
    pdjson_set_language(&parser, PDJSON_LANGUAGE_JSON5);
    next = pdjson_next(&parser);
    result = xjson5_value(&parser, next, 0u, &items);
    if (result) {
        next = pdjson_next(&parser);
        if (next != PDJSON_DONE) {
            xrtValueRelease(result);
            result = NULL;
            if (next == PDJSON_ERROR) xjson5_parse_error(&parser);
            else xjson5_error("trailing JSON5 content");
        }
    }
    pdjson_close(&parser);
    if (!result) {
        if (!xrtGetError()) xjson5_error("JSON5 parsing failed");
        return xrtValueRetain(xrtValueNull());
    }
    xrtClearError();
    return result;
}

bool xjson5_valid_len(const char* text, size_t size) {
    xvalue* value = xjson5_parse_len(text, size);
    const xerror* error = xrtGetError();
    bool valid = error == NULL;
    bool memory_failed = error != NULL && xrtErrorKind(error) == XERR_MEMORY;
    xrtValueRelease(value);
    if (!memory_failed) xrtClearError();
    return valid;
}

xvalue* xjson5_get_value_len(const char* text, size_t size, const char* path) {
    xvalue* root = xjson5_parse_len(text, size);
    const xvalue* current = root;
    const char* cursor = path;
    if (xrtGetError()) { xrtValueRelease(root); return xrtValueRetain(xrtValueNull()); }
    if (!path) {
        xrtValueRelease(root);
        xjson5_error("JSON5 path is missing");
        return xrtValueRetain(xrtValueNull());
    }
    while (*cursor) {
        const char* end = strchr(cursor, '.');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        if (length == 0) { current = NULL; break; }
        if (xrtValueType(current) == XVALUE_OBJECT) {
            current = xrtValueObjectGet(current, (xstrview){ cursor, length });
        } else if (xrtValueType(current) == XVALUE_ARRAY) {
            uint64 index;
            if (!xrtUIntParse((xstrview){ cursor, length }, 10u, 0u, &index) ||
                index > SIZE_MAX) { current = NULL; break; }
            current = xrtValueArrayGet(current, (size_t)index);
        } else { current = NULL; break; }
        if (!current) break;
        cursor = end ? end + 1 : cursor + length;
    }
    if (!current || (path[0] && cursor > path && cursor[-1] == '.')) {
        xrtValueRelease(root);
        xjson5_error("JSON5 path does not exist");
        return xrtValueRetain(xrtValueNull());
    }
    xvalue* selected = xrtValueRetain(current);
    xrtValueRelease(root);
    xrtClearError();
    return selected;
}

str xjson5_get_text_len(const char* text, size_t size, const char* path) {
    xvalue* value = xjson5_get_value_len(text, size, path);
    xstrview string;
    int64 integer;
    double number;
    bool boolean;
    str result = NULL;
    if (xrtGetError()) { xrtValueRelease(value); return NULL; }
    switch (xrtValueType(value)) {
        case XVALUE_STRING:
            if (xrtValueGetString(value, &string) &&
                memchr(string.Data, 0, string.Size) == NULL)
                result = xrtStrDupView(string);
            else xjson5_error("JSON5 string contains NUL; use getValue");
            break;
        case XVALUE_INT:
            if (xrtValueGetInt(value, &integer)) result = xrtIntString(integer, 10u, 0u);
            break;
        case XVALUE_FLOAT:
            if (xrtValueGetFloat(value, &number)) result = xrtNumString(number, 0u);
            break;
        case XVALUE_BOOL:
            if (xrtValueGetBool(value, &boolean)) result = xrtStrDup(boolean ? "true" : "false");
            break;
        case XVALUE_NULL:
            result = xrtStrDup("null");
            break;
        default:
            xjson5_error("JSON5 path contains an array or object; use getValue");
            break;
    }
    xrtValueRelease(value);
    return result;
}

bool xjson5_has_error(void) { return xrtGetError() != NULL; }

cstr xjson5_error_message(void) {
    const xerror* error = xrtGetError();
    return error ? xrtErrorMessage(error) : "";
}
