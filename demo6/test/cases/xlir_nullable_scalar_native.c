#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_stack.h"
#include "test/support/stack_take_found.h"
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
static void error(xerrkind kind)
{ assert(xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void numericBoundaries(void)
{
    const float floats[]={0.0f,-0.0f,NAN,INFINITY,-INFINITY};
    const double doubles[]={0.0,-0.0,NAN,INFINITY,-INFINITY};
    for (size_t i=0;i<sizeof(floats)/sizeof(*floats);++i) {
        XL6_NullableF32V1 a={true,floats[i]},b={0};
        xvalue* boxed=xrtValueFromTyped(xl6NullableF32TypeV1(),&a,xl6NullableF32ConverterV1());
        assert(boxed && xrtValueToTyped(boxed,xl6NullableF32TypeV1(),&b,xl6NullableF32ConverterV1()));
        equal(xl6NullableF32TypeV1(),&a,&b);
        if (floats[i]==0) assert(!!signbit(a.value)==!!signbit(b.value));
        xrtValueRelease(boxed);
        XL6_NullableF64V1 c={true,doubles[i]},d={0};
        boxed=xrtValueFromTyped(xl6NullableF64TypeV1(),&c,xl6NullableF64ConverterV1());
        assert(boxed && xrtValueToTyped(boxed,xl6NullableF64TypeV1(),&d,xl6NullableF64ConverterV1()));
        equal(xl6NullableF64TypeV1(),&c,&d);
        if (doubles[i]==0) assert(!!signbit(c.value)==!!signbit(d.value));
        xrtValueRelease(boxed);
    }
    XL6_NullableF32V1 f={true,73.0f}; xvalue* boxed=xrtValueFloat(0.1);
    assert(boxed && !xrtValueToTyped(boxed,xl6NullableF32TypeV1(),&f,xl6NullableF32ConverterV1()) && !f.hasValue);
    error(XERR_RANGE); xrtValueRelease(boxed);
    XL6_NullableI64V1 i={true,73}; boxed=xrtValueUInt(UINT64_MAX);
    assert(boxed && !xrtValueToTyped(boxed,xl6NullableI64TypeV1(),&i,xl6NullableI64ConverterV1()) && !i.hasValue);
    error(XERR_RANGE); xrtValueRelease(boxed);
    XL6_NullableU64V1 u={true,73}; boxed=xrtValueInt(-1);
    assert(boxed && !xrtValueToTyped(boxed,xl6NullableU64TypeV1(),&u,xl6NullableU64ConverterV1()) && !u.hasValue);
    error(XERR_RANGE);
    XL6_NullableTimeV1 t={true,73};
    assert(!xrtValueToTyped(boxed,xl6NullableTimeTypeV1(),&t,xl6NullableTimeConverterV1()) && !t.hasValue);
    error(XERR_TYPE); xrtValueRelease(boxed);
    XL6_NullableCharV1 c={true,73}; boxed=xrtValueUInt(65);
    assert(boxed && !xrtValueToTyped(boxed,xl6NullableCharTypeV1(),&c,xl6NullableCharConverterV1()) && !c.hasValue);
    error(XERR_TYPE); xrtValueRelease(boxed);
    c=(XL6_NullableCharV1){true,0xD800};
    assert(!xrtValueFromTyped(xl6NullableCharTypeV1(),&c,xl6NullableCharConverterV1())); error(XERR_VALUE);
    c.value=0x110000;
    assert(!xrtValueFromTyped(xl6NullableCharTypeV1(),&c,xl6NullableCharConverterV1())); error(XERR_VALUE);
    XL6_NullableBoolV1 flag={true,true};
    boxed=xrtValueFromTyped(xl6NullableBoolTypeV1(),&flag,xl6NullableBoolConverterV1());
    assert(boxed==xrtValueBool(true) && !xrtValueTypeId(boxed)); xrtValueRelease(boxed);
    flag.hasValue=false;
    boxed=xrtValueFromTyped(xl6NullableBoolTypeV1(),&flag,xl6NullableBoolConverterV1());
    assert(boxed==xrtValueNull() && !xrtValueTypeId(boxed)); xrtValueRelease(boxed);
}
#define CHECK_VALUE_PROFILE(CARRIER, TYPE, CONVERTER, KIND, FIRST, SECOND) do { \
    const xrttype* type=TYPE(); const xvalueconverter* converter=CONVERTER(); \
    assert(converter==CONVERTER() && converter->ToTyped && converter->FromTyped); \
    CARRIER absent={0},a={true,FIRST},b={true,SECOND},output={0}; \
    CARRIER vectors[]={absent,a,b}; \
    for (size_t i=0;i<3;++i) { \
        xvalue* boxed=xrtValueFromTyped(type,&vectors[i],converter); assert(boxed); \
        assert(xrtValueType(boxed)==(i ? KIND : XVALUE_NULL)); \
        assert(xrtMemDebugFailAfter(0)); \
        assert(xrtValueToTyped(boxed,type,&output,converter)); \
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); \
        equal(type,&output,&vectors[i]); xrtValueRelease(boxed); \
    } \
    assert(xrtMemDebugFailAfter(0)); \
    assert(xrtValueFromTyped(type,&absent,converter)==xrtValueNull()); \
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); \
    size_t terminal=0; \
    for (;;++terminal) { \
        assert(terminal<32 && xrtMemDebugFailAfter(terminal)); \
        xvalue* boxed=xrtValueFromTyped(type,&a,converter); \
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); \
        if (hit) { assert(!boxed); error(XERR_MEMORY); } \
        else { assert(boxed && !xrtGetError()); xrtValueRelease(boxed); break; } \
    } \
    xvalue* bad=xrtValueString((xstrview)XRT_STR_INIT("reject")); assert(bad); \
    assert(!xrtValueToTyped(bad,type,&output,converter) && !output.hasValue); error(XERR_TYPE); \
    output=b; xrttype wrong=*type; \
    assert(!converter->ToTyped(bad,&wrong,&output,NULL)); error(XERR_TYPE); equal(type,&output,&b); \
    assert(!converter->FromTyped(&wrong,&b,NULL)); error(XERR_TYPE); \
    xrtValueRelease(bad); \
    printf("nullable projection %s complete-fault-prefix=%zu\n",#TYPE,terminal); \
} while (0)
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
    assert(xl6StackFoundTest(stack, 101, type->Id, 1, sizeof(CARRIER), type, 0, &moved, true)); \
    equal(type, &a, &moved); \
    assert(xl6StackFoundTest(stack, 101, type->Id, 1, sizeof(CARRIER), type, 0, &moved, true)); \
    assert(!moved.hasValue); \
    assert(!xl6StackFoundTest(stack, 101, type->Id, 1, sizeof(CARRIER), type, 0, &moved, true)); \
    assert(!xrtGetError()); xrtValueRelease(clone); xrtValueRelease(stack); \
} while (0)

int main(void)
{
    assert(xrtMemDebugEnable(true));
    xmemdebugsnapshot before, after;
    xrtMemDebugSnapshot(&before);
    CHECK_PROFILE(XL6_NullableBoolV1, xl6NullableBoolTypeV1, xrtTypeBool, false, true);
    CHECK_PROFILE(XL6_NullableI64V1, xl6NullableI64TypeV1, xrtTypeInt64, INT64_MIN, INT64_MAX);
    CHECK_PROFILE(XL6_NullableU64V1, xl6NullableU64TypeV1, xrtTypeUInt64, 0, UINT64_MAX);
    CHECK_PROFILE(XL6_NullableF32V1, xl6NullableF32TypeV1, xrtTypeFloat32, -1.25f, 1.25f);
    CHECK_PROFILE(XL6_NullableF64V1, xl6NullableF64TypeV1, xrtTypeFloat64, -1.25, 1.25);
    CHECK_PROFILE(XL6_NullableTimeV1, xl6NullableTimeTypeV1, xrtTypeTime, INT64_MIN, INT64_MAX);
    CHECK_PROFILE(XL6_NullableCharV1, xl6NullableCharTypeV1, xrtTypeUInt32, 0, 0x10FFFF);
    CHECK_VALUE_PROFILE(XL6_NullableBoolV1,xl6NullableBoolTypeV1,xl6NullableBoolConverterV1,XVALUE_BOOL,false,true);
    CHECK_VALUE_PROFILE(XL6_NullableI64V1,xl6NullableI64TypeV1,xl6NullableI64ConverterV1,XVALUE_INT,INT64_MIN,INT64_MAX);
    CHECK_VALUE_PROFILE(XL6_NullableU64V1,xl6NullableU64TypeV1,xl6NullableU64ConverterV1,XVALUE_UINT,0,UINT64_MAX);
    CHECK_VALUE_PROFILE(XL6_NullableF32V1,xl6NullableF32TypeV1,xl6NullableF32ConverterV1,XVALUE_FLOAT,-1.25f,1.25f);
    CHECK_VALUE_PROFILE(XL6_NullableF64V1,xl6NullableF64TypeV1,xl6NullableF64ConverterV1,XVALUE_FLOAT,-1.25,1.25);
    CHECK_VALUE_PROFILE(XL6_NullableTimeV1,xl6NullableTimeTypeV1,xl6NullableTimeConverterV1,XVALUE_TIME,INT64_MIN,INT64_MAX);
    CHECK_VALUE_PROFILE(XL6_NullableCharV1,xl6NullableCharTypeV1,xl6NullableCharConverterV1,XVALUE_CHAR,0,0x10FFFF);
    numericBoundaries();
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
