#include "src/runtime/xl6_integer_convert.h"
#include "src/runtime/xl6_native_scalar_profile.h"
#include "src/runtime/xl6_number_internal.h"
#include "src/api/xl_foreign_error.h"

static const xrttype* integerTarget(unsigned profile)
{
    switch (profile) {
    case XL6_INTEGER_SIGNED_8: return xrtTypeInt8();
    case XL6_INTEGER_UNSIGNED_8: return xrtTypeUInt8();
    case XL6_INTEGER_SIGNED_16: return xrtTypeInt16();
    case XL6_INTEGER_UNSIGNED_16: return xrtTypeUInt16();
    case XL6_INTEGER_SIGNED_32: return xrtTypeInt32();
    case XL6_INTEGER_UNSIGNED_32: return xrtTypeUInt32();
    case XL6_INTEGER_SIGNED_64: return xrtTypeInt64();
    case XL6_INTEGER_UNSIGNED_64: return xrtTypeUInt64();
    default: return NULL;
    }
}

static void publish(unsigned profile, const XL6_NativeScalarScratchV1* value, void* output)
{
    int64_t integer = 0;
    switch (profile) {
    case XL6_INTEGER_SIGNED_8: integer = value->i8; break;
    case XL6_INTEGER_UNSIGNED_8: integer = value->u8; break;
    case XL6_INTEGER_SIGNED_16: integer = value->i16; break;
    case XL6_INTEGER_UNSIGNED_16: integer = value->u16; break;
    case XL6_INTEGER_SIGNED_32: integer = value->i32; break;
    case XL6_INTEGER_UNSIGNED_32: integer = value->u32; break;
    case XL6_INTEGER_SIGNED_64: integer = value->i64; break;
    case XL6_INTEGER_UNSIGNED_64: memcpy(output, &value->u64, sizeof(value->u64)); return;
    }
    memcpy(output, &integer, sizeof(integer));
}

static bool parse(xstrview text, unsigned profile, XL6_NativeScalarScratchV1* value)
{
    const xrttype* target = integerTarget(profile);
    if (!target || (!text.Data && text.Size)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    if (!text.Data) text.Data = "";
    if (!(profile & 1u)) {
        uint64_t number = 0;
        if (!xrtUIntParse(text, 10, XNUMBER_PARSE_SPACE, &number)) { xl6NumberParseError(); return false; }
        return xrtTypeConvert(xrtTypeUInt64(), &number, target, value, XTYPE_CONVERT_EXPLICIT);
    }
    int64_t number = 0;
    if (!xrtIntParse(text, 10, XNUMBER_PARSE_SPACE, &number)) { xl6NumberParseError(); return false; }
    return xrtTypeConvert(xrtTypeInt64(), &number, target, value, XTYPE_CONVERT_EXPLICIT);
}

static bool finish(xl6_error_scope* scope, bool ok, unsigned profile,
    const XL6_NativeScalarScratchV1* value, void* output)
{
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(scope);
    xl6_error_result result = xl6ErrorScopeEnd(scope);
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); return false; }
    publish(profile, value, output);
    return true;
}

static bool begin(xl6_error_scope* scope)
{
    if (xl6ErrorScopeBegin(scope)) return true;
    xl6ErrorScopePropagateFailure((xl6_error_result){false, xrtTakeError()});
    return false;
}

bool xl6IntegerTextConvertV1(xstrview text, unsigned profile, void* output)
{
    xl6_error_scope scope = {0}; XL6_NativeScalarScratchV1 value = {0};
    if (!begin(&scope)) return false;
    bool ok = false;
    if (!output) xrtSetErrorKind(XERR_ARGUMENT);
    else ok = parse(text, profile, &value);
    return finish(&scope, ok, profile, &value, output);
}

bool xl6IntegerValueConvertV1(const xvalue* source, unsigned profile, uint64_t reflectionIdentity, void* output)
{
    xl6_error_scope scope = {0}; XL6_NativeScalarScratchV1 input = {0}, value = {0};
    if (!begin(&scope)) return false;
    const xrttype* target = integerTarget(profile); const xrttype* from = NULL; bool ok = false;
    if (!target || !reflectionIdentity || !output) xrtSetErrorKind(XERR_ARGUMENT);
    else {
        xvaluetype tag = source ? xrtValueType(source) : XVALUE_NULL;
        if (tag == XVALUE_INVALID) xl6ErrorScopeCaptureFailure(&scope);
        else if (source && xrtValueTypeId(source) == reflectionIdentity) xrtSetErrorKind(XERR_TYPE);
        else switch (tag) {
        case XVALUE_NULL: from = xrtTypeInt64(); ok = true; break;
        case XVALUE_BOOL: from = xrtTypeBool(); ok = xrtValueGetBool(source, &input.boolean); break;
        case XVALUE_INT: from = xrtTypeInt64(); ok = xrtValueGetInt(source, &input.i64); break;
        case XVALUE_UINT: from = xrtTypeUInt64(); ok = xrtValueGetUInt(source, &input.u64); break;
        case XVALUE_FLOAT: from = xrtTypeFloat64(); ok = xrtValueGetFloat(source, &input.f64); break;
        case XVALUE_STRING: {
            xstrview text = {0};
            ok = xrtValueGetString(source, &text) && parse(text, profile, &value);
            break;
        }
        default: xrtSetErrorKind(XERR_TYPE); break;
        }
        if (ok && from) ok = xrtTypeConvert(from, &input, target, &value, XTYPE_CONVERT_EXPLICIT);
    }
    return finish(&scope, ok, profile, &value, output);
}
