#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_stack.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64 identity(xstrview name)
{
    uint64 result = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < name.Size; ++i) {
        result ^= (unsigned char)name.Data[i];
        result *= UINT64_C(1099511628211);
    }
    return result;
}
static void equal(const xrttype* type, const void* left, const void* right)
{
    int comparison = 73; uint64 a = 0, b = 1;
    assert(xrtTypeCompareValue(type, left, right, &comparison) && comparison == 0);
    assert(xrtTypeHashValue(type, left, &a) && xrtTypeHashValue(type, right, &b) && a == b);
}
#define CHECK_PROFILE(CARRIER, GETTER, ELEMENT, FIRST, SECOND) do { \
    const xrttype* type = GETTER(); \
    assert(type == GETTER() && xrtTypeValidate(type)); \
    assert(type->Id == identity(type->AbiName) && type->Kind == XRT_TYPE_OPTIONAL); \
    assert(type->Size == sizeof(CARRIER) && type->Align == _Alignof(CARRIER)); \
    assert(xrtTypeIsCopyable(type) && xrtTypeIsRelocatable(type)); \
    assert(xrtTypeIsComparable(type) && xrtTypeIsHashable(type)); \
    CARRIER absentA, absentB, a, b, copied, moved; \
    memset(&absentA, 0x35, sizeof(absentA)); absentA.hasValue = false; \
    memset(&absentB, 0xA9, sizeof(absentB)); absentB.hasValue = false; \
    equal(type, &absentA, &absentB); \
    memset(&a, 0x35, sizeof(a)); a.hasValue = true; a.value = (FIRST); \
    memset(&b, 0xA9, sizeof(b)); b.hasValue = true; b.value = (FIRST); \
    equal(type, &a, &b); \
    int order, scalarOrder; uint64 hash, scalarHash; \
    b.value = (SECOND); \
    assert(xrtTypeCompareValue(type, &a, &b, &order)); \
    assert(xrtTypeCompareValue(ELEMENT(), &a.value, &b.value, &scalarOrder)); \
    assert(order == scalarOrder); \
    assert(xrtTypeCompareValue(type, &absentA, &a, &order) && order < 0); \
    assert(xrtTypeCompareValue(type, &a, &absentA, &order) && order > 0); \
    assert(xrtTypeHashValue(type, &a, &hash)); \
    assert(xrtTypeHashValue(ELEMENT(), &a.value, &scalarHash)); \
    assert(hash == (scalarHash ^ UINT64_C(0x9E3779B97F4A7C15))); \
    assert(xrtTypeInitValue(type, &copied) && !copied.hasValue); \
    assert(xrtTypeCopyValue(type, &copied, &a)); equal(type, &a, &copied); \
    assert(xrtTypeInitValue(type, &moved)); \
    assert(xrtTypeMoveValue(type, &moved, &copied)); \
    assert(!copied.hasValue); equal(type, &a, &moved); \
    xrtTypeDropValue(type, &moved); \
    xvalue* stack = xl6StackCreateV1(101, type->Id, 1, sizeof(CARRIER), type); \
    assert(stack && xl6StackPushV1(stack, 101, type->Id, 1, sizeof(CARRIER), type, &absentA)); \
    assert(xl6StackPushV1(stack, 101, type->Id, 1, sizeof(CARRIER), type, &a)); \
    xvalue* clone = xl6StackCloneV1(stack, 101, type->Id, 1, sizeof(CARRIER), type); \
    assert(clone && xl6StackEqualsV1(stack, clone, 101, type->Id, 1, sizeof(CARRIER), type)); \
    assert(xl6StackTryTakeV1(stack, 101, type->Id, 1, sizeof(CARRIER), type, 0, &moved, true)); \
    equal(type, &a, &moved); \
    assert(xl6StackTryTakeV1(stack, 101, type->Id, 1, sizeof(CARRIER), type, 0, &moved, true)); \
    assert(!moved.hasValue); \
    assert(!xl6StackTryTakeV1(stack, 101, type->Id, 1, sizeof(CARRIER), type, 0, &moved, true)); \
    assert(!xrtGetError()); xrtValueRelease(clone); xrtValueRelease(stack); \
} while (0)

int main(void)
{
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    CHECK_PROFILE(XL6_NullableBoolV1, xl6NullableBoolTypeV1, xrtTypeBool, false, true);
    CHECK_PROFILE(XL6_NullableI64V1, xl6NullableI64TypeV1, xrtTypeInt64, INT64_MIN, INT64_MAX);
    CHECK_PROFILE(XL6_NullableU64V1, xl6NullableU64TypeV1, xrtTypeUInt64, 0, UINT64_MAX);
    CHECK_PROFILE(XL6_NullableF32V1, xl6NullableF32TypeV1, xrtTypeFloat32, -1.25f, 1.25f);
    CHECK_PROFILE(XL6_NullableF64V1, xl6NullableF64TypeV1, xrtTypeFloat64, -1.25, 1.25);
    CHECK_PROFILE(XL6_NullableTimeV1, xl6NullableTimeTypeV1, xrtTypeTime, INT64_MIN, INT64_MAX);
    CHECK_PROFILE(XL6_NullableCharV1, xl6NullableCharTypeV1, xrtTypeUInt32, 0, 0x10FFFF);
    XL6_NullableF32V1 fz = {true, 0.0f}, fnz = {true, -0.0f}, fnan = {true, NAN};
    XL6_NullableF64V1 dz = {true, 0.0}, dnz = {true, -0.0}, dnan = {true, NAN};
    equal(xl6NullableF32TypeV1(), &fz, &fnz); equal(xl6NullableF32TypeV1(), &fnan, &fnan);
    equal(xl6NullableF64TypeV1(), &dz, &dnz); equal(xl6NullableF64TypeV1(), &dnan, &dnan);
    const xrttype* types[] = {xl6NullableBoolTypeV1(), xl6NullableI64TypeV1(), xl6NullableU64TypeV1(),
        xl6NullableF32TypeV1(), xl6NullableF64TypeV1(), xl6NullableTimeTypeV1(), xl6NullableCharTypeV1()};
    for (size_t i = 0; i < sizeof(types)/sizeof(*types); ++i)
        for (size_t j = i + 1; j < sizeof(types)/sizeof(*types); ++j) assert(!xrtTypeSame(types[i], types[j]));
    xrtSetErrorKind(XERR_STATE);
    const xerror* saved = xrtGetError();
    assert(saved && xrtMemDebugFailAfter(0));
    assert(types[0] == xl6NullableBoolTypeV1() && types[1] == xl6NullableI64TypeV1());
    assert(types[2] == xl6NullableU64TypeV1() && types[3] == xl6NullableF32TypeV1());
    assert(types[4] == xl6NullableF64TypeV1() && types[5] == xl6NullableTimeTypeV1());
    assert(types[6] == xl6NullableCharTypeV1());
    equal(types[3], &fz, &fnz); equal(types[4], &dnan, &dnan);
    assert(!xrtMemDebugFailTriggered() && saved == xrtGetError());
    xrtMemDebugFailClear(); xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    puts("nullable scalar native: seven immutable identities/layouts, typed Stack, absent/padding, move/copy, scalar compare/hash, zero/NaN, allocation-free descriptors, balanced owners");
    return 0;
}
