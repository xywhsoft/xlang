#include "src/runtime/xl6_class_text.h"
#include "src/runtime/xl6_value_format.h"
#include "src/runtime/xl6_text_internal.h"

xstrview xl6ClassValueTextV1(const xvalue* value, const XL6_ClassTextSchemaV1* schema)
{
    if (!schema || !schema->Name.Data || schema->RuntimeTypeTag <= 0 || !schema->TypeId ||
        (schema->FieldCount && !schema->Fields)) {
        xrtSetErrorKind(XERR_ARGUMENT);
        return xl6TextFailure;
    }
    if (!value || xrtValueType(value) == XVALUE_NULL)
        return xl6TextCopy(xrtStrView("null"));

    int64_t tag = 0;
    if (xrtValueType(value) != XVALUE_OBJECT || xrtValueTypeId(value) != schema->TypeId ||
        !xrtValueGetInt(xrtValueObjectGet(value, xrtStrView("$xlir.class.runtime-type")), &tag) ||
        tag != schema->RuntimeTypeTag) {
        xrtSetErrorKind(XERR_TYPE);
        return xl6TextFailure;
    }
    xstrbuf output;
    xrtStrBufInit(&output);
    bool ready = xrtStrBufAppend(&output, schema->Name) &&
        xrtStrBufAppend(&output, xrtStrView("{"));
    for (size_t i = 0; ready && i < schema->FieldCount; ++i) {
        xstrview name = schema->Fields[i];
        if (!name.Data) { xrtSetErrorKind(XERR_ARGUMENT); ready = false; break; }
        const xvalue* item = xrtValueObjectGet(value, name);
        if (!item) { xrtSetErrorKind(XERR_STATE); ready = false; break; }
        ready = (!i || xrtStrBufAppend(&output, xrtStrView(", "))) &&
            xrtStrBufAppend(&output, name) && xrtStrBufAppend(&output, xrtStrView("=")) &&
            xl6ValueTextAppendV1(&output, item);
    }
    if (ready) ready = xrtStrBufAppend(&output, xrtStrView("}"));
    size_t size = output.Size;
    char* text = ready ? xrtStrBufTake(&output) : NULL;
    /* This builder owns raw storage only: cleanup cannot call user destructors
     * or replace the formatter's first error with a retirement error. */
    xrtStrBufFree(&output);
    return ready ? xl6TextOwn(text, size) : xl6TextFailure;
}
