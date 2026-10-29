#include "src/runtime/xl6_float_convert.h"
#include "src/runtime/xl6_number_internal.h"
#include "src/api/xl_foreign_error.h"
#include <math.h>

static bool narrow(double source, float* output)
{
    /* The supported targets use IEEE binary32/64. Preserve special values;
     * XRT's explicit conversion rejects finite out-of-range values and rounds
     * representable values, rather than the lossless typed-read policy. */
    if (isinf(source)) { *output = signbit(source) ? -INFINITY : INFINITY; return true; }
    return xrtTypeConvert(xrtTypeFloat64(), &source,
        xrtTypeFloat32(), output, XTYPE_CONVERT_EXPLICIT);
}

static bool finish(xl6_error_scope* scope, bool ok, float value, float* output)
{
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result = xl6ErrorScopeEnd(scope);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); return false; }
    *output = value;
    return true;
}

static bool begin(xl6_error_scope* scope)
{
    if (xl6ErrorScopeBegin(scope)) return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false, xrtTakeError()});
    return false;
}

bool xl6Float32ValueConvertV2(const xvalue* source, uint64_t reflectionIdentity, float* output)
{
    xl6_error_scope scope = {0}; float value = 0; bool ok = false;
    if (!begin(&scope)) return false;
    if (!output || !reflectionIdentity) xrtSetErrorKind(XERR_ARGUMENT);
    else if (!source) xrtSetErrorKind(XERR_TYPE);
    else if (xrtValueTypeId(source) == reflectionIdentity) xrtSetErrorKind(XERR_TYPE);
    else if (xrtValueType(source) == XVALUE_STRING) {
        xstrview text = {0};
        ok = xrtValueGetString(source, &text);
        if (!ok) xl6ErrorScopeCaptureFailure(&scope);
        else ok = xl6Float32TextConvertV1(text, &value);
    } else if (xrtValueType(source) == XVALUE_FLOAT) {
        double number = 0;
        ok = xrtValueGetFloat(source, &number);
        if (!ok) xl6ErrorScopeCaptureFailure(&scope);
        else ok = narrow(number, &value);
    } else if (xrtValueType(source) == XVALUE_INT || xrtValueType(source) == XVALUE_TIME) {
        int64_t number = 0;
        ok = xrtValueType(source) == XVALUE_INT
            ? xrtValueGetInt(source, &number) : xrtValueGetTime(source, &number);
        if (!ok) xl6ErrorScopeCaptureFailure(&scope);
        /* All 64-bit integers fit binary32's finite range. Convert directly:
         * an intermediate double can round a large integer to a binary32
         * halfway value and then round it a second time in the wrong direction. */
        else value = (float)number;
    } else if (xrtValueType(source) == XVALUE_UINT) {
        uint64_t number = 0;
        ok = xrtValueGetUInt(source, &number);
        if (!ok) xl6ErrorScopeCaptureFailure(&scope);
        else value = (float)number;
    } else ok = xrtValueConvertTo(source, xrtTypeFloat32(), &value, XTYPE_CONVERT_EXPLICIT);
    return finish(&scope, ok, value, output);
}

bool xl6Float32TextConvertV1(xstrview text, float* output)
{
    xl6_error_scope scope = {0}; float value = 0; double parsed = 0; bool ok = false;
    if (!begin(&scope)) return false;
    if (!output || (!text.Data && text.Size)) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        if (!text.Data) text.Data = "";
        ok = xrtNumParse(text, XNUMBER_PARSE_SPACE | XNUMBER_PARSE_SPECIAL, &parsed);
        if (!ok) { xl6NumberParseError(); xl6ErrorScopeCaptureFailure(&scope); }
        else ok = narrow(parsed, &value);
    }
    return finish(&scope, ok, value, output);
}

bool xl6Float32ConvertV1(double source, float* output)
{
    xl6_error_scope scope = {0}; float value = 0; bool ok = false;
    if (!begin(&scope)) return false;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    else ok = narrow(source, &value);
    return finish(&scope, ok, value, output);
}

bool xl6Float64TextConvertV1(xstrview text, double* output)
{
    xl6_error_scope scope = {0}; double value = 0; bool ok = false;
    if (!begin(&scope)) return false;
    if (!output || (!text.Data && text.Size)) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        if (!text.Data) text.Data = "";
        ok = xrtNumParse(text, XNUMBER_PARSE_SPACE | XNUMBER_PARSE_SPECIAL, &value);
        if (!ok) xl6NumberParseError();
    }
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result result = xl6ErrorScopeEnd(&scope);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); return false; }
    *output = value;
    return true;
}
