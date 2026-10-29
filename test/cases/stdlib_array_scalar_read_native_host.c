#include "src/runtime/xl6_array_scalar_read.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#define IDS UINT64_C(61001), UINT64_C(61002)
#define LAYOUT(Type, CType) IDS, 1, sizeof(CType), Type()
static void error(xerrkind expected)
{
    if (!xrtGetError() || xrtErrorKind(xrtGetError()) != expected)
        fprintf(stderr, "read error expected=%d actual=%d\n", (int)expected,
            xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : -1);
    assert(xrtGetError() && xrtErrorKind(xrtGetError()) == expected);
    xrtClearError();
}
static xvalue* source(const xrttype* type, size_t size, const void* values)
{
    xvalue* result = xl6ArrayCreateV1(IDS, 1, size, type); assert(result);
    for (unsigned i = 0; i < 3; ++i)
        assert(xl6ArrayPushV1(result, IDS, 1, size, type, (const char*)values + i * size));
    xvalue* alias = xl6ArrayEraseV1(result, IDS, 1, size, type, NULL);
    assert(alias == result); xrtValueRelease(alias); return result;
}
static void drop(void* value, void* context) { (void)value; (void)context; }
static const xvaluehandleops foreignOps = {NULL, drop, NULL, NULL};
static void rejected(const xvalue* value, xerrkind kind)
{
    assert(xl6ArrayReadI64V1(value, IDS, 0) == 0); error(kind);
    assert(xl6ArrayReadU64V1(value, IDS, 0) == 0); error(kind);
    assert(!xl6ArrayReadBoolV1(value, IDS, 0)); error(kind);
    assert(xl6ArrayReadF64V1(value, IDS, 0) == 0); error(kind);
}
static void vectors(void)
{
    int64_t integers[] = {0, INT64_MIN, INT64_MAX};
    uint64_t unsigneds[] = {0, UINT64_MAX, 9};
    bool booleans[] = {false, true, false};
    double floats[] = {-0.0, INFINITY, NAN};
    xvalue* i = source(xrtTypeInt64(), sizeof(int64_t), integers);
    xvalue* u = source(xrtTypeUInt64(), sizeof(uint64_t), unsigneds);
    xvalue* b = source(xrtTypeBool(), sizeof(bool), booleans);
    xvalue* f = source(xrtTypeFloat64(), sizeof(double), floats);
    xvalue* alias = xrtValueRetain(i);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ArrayReadI64V1(alias, IDS, -2) == INT64_MIN);
    assert(xl6ArrayReadI64V1(i, IDS, -1) == INT64_MAX);
    assert(xl6ArrayReadU64V1(u, IDS, 1) == UINT64_MAX);
    assert(xl6ArrayReadBoolV1(b, IDS, 1) && !xl6ArrayReadBoolV1(b, IDS, 0));
    double zero = xl6ArrayReadF64V1(f, IDS, 0);
    assert(signbit(zero) && isinf(xl6ArrayReadF64V1(f, IDS, 1)) && isnan(xl6ArrayReadF64V1(f, IDS, -1)));
    assert(!xrtGetError() && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
#define CHECK(Value, Type, CType, Read) do { \
    XL6_Sequence* pin = xl6SequenceBeginV1(Value, LAYOUT(Type, CType)); assert(pin); \
    rejected(Value, XERR_STATE); xl6SequenceEndV1(pin); \
    assert(Read(Value, IDS, 3) == 0); error(XERR_RANGE); \
    assert(Read(Value, IDS, INT64_MIN) == 0); error(XERR_RANGE); \
    assert(Read(Value, 0, 61002, 0) == 0); error(XERR_ARGUMENT); \
    assert(Read(Value, 61001, 0, 0) == 0); error(XERR_ARGUMENT); \
} while (0)
    CHECK(i, xrtTypeInt64, int64_t, xl6ArrayReadI64V1);
    CHECK(u, xrtTypeUInt64, uint64_t, xl6ArrayReadU64V1);
    CHECK(b, xrtTypeBool, bool, xl6ArrayReadBoolV1);
    CHECK(f, xrtTypeFloat64, double, xl6ArrayReadF64V1);
#undef CHECK
    assert(xl6ArrayReadI64V1(i, 61000, 61002, 0) == 0); error(XERR_TYPE);
    assert(xl6ArrayReadI64V1(i, 61001, 61000, 0) == 0); error(XERR_TYPE);
    assert(xl6ArrayReadI64V1(i, IDS, 3) == 0); error(XERR_RANGE);
    assert(xl6ArrayReadI64V1(i, IDS, INT64_MIN) == 0); error(XERR_RANGE);
    assert(xl6ArrayReadF64V1(i, IDS, 0) == 0); error(XERR_TYPE);
    xvalue* wire = xrtValueArray(); assert(wire);
    assert(xrtValueArrayAppendNew(wire, xrtValueInt(INT64_MIN)));
    assert(xrtValueArrayAppendNew(wire, xrtValueUInt(UINT64_MAX)));
    assert(xrtValueArrayAppendNew(wire, xrtValueBool(true)));
    assert(xrtValueArrayAppendNew(wire, xrtValueFloat(-0.0)));
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ArrayReadI64V1(wire, IDS, -4) == INT64_MIN);
    assert(xl6ArrayReadU64V1(wire, IDS, 1) == UINT64_MAX);
    assert(xl6ArrayReadBoolV1(wire, IDS, 2));
    assert(signbit(xl6ArrayReadF64V1(wire, IDS, -1)));
    assert(!xrtGetError() && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(!xl6ArrayReadBoolV1(wire, IDS, 0)); error(XERR_TYPE);
    assert(!xl6ArrayReadI64V1(wire, IDS, 4)); error(XERR_RANGE);
    rejected(NULL, XERR_ARGUMENT); rejected(xrtValueNull(), XERR_TYPE);
    xvalue* unbound = xl6ArrayCreateV1(LAYOUT(xrtTypeInt64, int64_t)); assert(unbound);
    rejected(unbound, XERR_STATE); xrtValueRelease(unbound);
    xvalue* empty = xl6ArrayCreateV1(LAYOUT(xrtTypeInt64, int64_t)); assert(empty);
    xvalue* erased = xl6ArrayEraseV1(empty, LAYOUT(xrtTypeInt64, int64_t), NULL); assert(erased == empty);
    assert(!xl6ArrayReadI64V1(empty, IDS, -1)); error(XERR_RANGE);
    xrtValueRelease(erased); xrtValueRelease(empty);
    xvalue* stack = xl6SequenceWrapAsV1(xrtTypedArrayCreate(xrtTypeInt64()),
        XL6_SEQUENCE_STACK_V1, LAYOUT(xrtTypeInt64, int64_t)); assert(stack);
    rejected(stack, XERR_TYPE); xrtValueRelease(stack);
    void* raw = (void*)(uintptr_t)1;
    xvalue* foreign = xrtValueHandleTake(&raw, &foreignOps, NULL); assert(foreign && !raw);
    assert(xrtValueTypeIdBind(foreign, 61001)); rejected(foreign, XERR_TYPE); xrtValueRelease(foreign);
    /* Same byte width and logical IDs never authorize another descriptor. */
    assert(!xl6ArrayReadI64V1(u, IDS, 0)); error(XERR_TYPE);
    xrtValueRelease(wire); xrtValueRelease(alias);
    xrtValueRelease(i); xrtValueRelease(u); xrtValueRelease(b); xrtValueRelease(f);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before); vectors(); xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
            before.UseAfterFreeCount == after.UseAfterFreeCount && !xrtGetError());
    }
    puts("scalar Array reads: 100 physical cycles balanced; native reads zero allocations"); return 0;
}
