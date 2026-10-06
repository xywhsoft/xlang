#include "src/runtime/xl6_text_internal.h"
#include "src/runtime/xl6_value_format.h"
#include <stdio.h>

typedef bool (*TextSink)(xstrview, void*);

static bool render(const xvalue* value, TextSink sink, void* output)
{
    char buffer[128];
    size_t size = 0;
    switch (value ? xrtValueType(value) : XVALUE_NULL) {
    case XVALUE_NULL: return sink(xrtStrView("null"), output);
    case XVALUE_BOOL: {
        bool item;
        if (!xrtValueGetBool(value, &item)) return false;
        return sink(xrtStrView(item ? "true" : "false"), output);
    }
    case XVALUE_INT: {
        int64_t item;
        if (!xrtValueGetInt(value, &item) ||
            !xrtIntFormatTo(item, xrtStrView(""), buffer, sizeof(buffer), &size))
            return false;
        return sink((xstrview){buffer, size}, output);
    }
    case XVALUE_UINT: {
        uint64_t item;
        if (!xrtValueGetUInt(value, &item) ||
            !xrtUIntFormatTo(item, xrtStrView(""), buffer, sizeof(buffer), &size))
            return false;
        return sink((xstrview){buffer, size}, output);
    }
    case XVALUE_FLOAT: {
        double item;
        if (!xrtValueGetFloat(value, &item) ||
            !xrtNumFormatTo(item, xrtStrView(""), buffer, sizeof(buffer), &size))
            return false;
        return sink((xstrview){buffer, size}, output);
    }
    case XVALUE_STRING: {
        xstrview item;
        if (!xrtValueGetString(value, &item)) return false;
        return sink(item, output);
    }
    case XVALUE_CHAR: {
        uint32_t item;
        if (!xrtValueGetChar(value, &item)) return false;
        size = xrtUtf8Encode(item, buffer);
        if (!size) { xrtSetErrorKind(XERR_VALUE); return false; }
        return sink((xstrview){buffer, size}, output);
    }
    case XVALUE_POINTER: {
        void* pointer = NULL;
        if (!xrtValueGetPointer(value, &pointer)) return false;
        int written = snprintf(buffer, sizeof(buffer), "%p", pointer);
        if (written < 0 || (size_t)written >= sizeof(buffer)) {
            xrtSetErrorKind(XERR_RANGE); return false;
        }
        return sink((xstrview){buffer, (size_t)written}, output);
    }
    case XVALUE_HANDLE: {
        const xerror* error = xl6ErrorValueBorrowV1(value);
        if (error) return sink(xrtStrView(xrtErrorMessage(error)), output);
        break;
    }
    default: break;
    }
    /* Serialization remains explicit and fallible. In particular an unknown
     * handle is not silently cast, replaced with a fake message or swallowed. */
    char* text = xrtXlonStringify(value, false, &size);
    if (!text) return false;
    bool ok = sink((xstrview){text, size}, output);
    xrtFree(text);
    return ok;
}
static bool copyText(xstrview text, void* output)
{
    xstrview copy = xl6TextCopy(text);
    if (!copy.Data) return false;
    *(xstrview*)output = copy;
    return true;
}
xstrview xl6ValueTextV1(const xvalue* value)
{
    xstrview result = xl6TextFailure;
    if (!render(value, copyText, &result)) return xl6TextFailure;
    return result;
}
static bool appendText(xstrview text, void* output)
{ return xrtStrBufAppend(output, text); }
bool xl6ValueTextAppendV1(xstrbuf* output, const xvalue* value)
{
    if (!output) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    return render(value, appendText, output);
}
