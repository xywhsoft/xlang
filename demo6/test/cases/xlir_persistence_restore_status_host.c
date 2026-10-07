#include <assert.h>
#include <stdio.h>
#include "test/support/module_product_memory.h"
#include "src/runtime/xl6_persistence_restore.h"

static xvalue* restoreTiny(const xvalue* value)
{
    return xl6PersistenceRestoreScalarV1(value, XL6_VALUE_ADMIT_I8, true);
}
static void scalarDomains(void)
{
    const unsigned rules[] = {XL6_VALUE_ADMIT_I8, XL6_VALUE_ADMIT_I16, XL6_VALUE_ADMIT_I32,
        XL6_VALUE_ADMIT_I64, XL6_VALUE_ADMIT_U8, XL6_VALUE_ADMIT_U16, XL6_VALUE_ADMIT_U32, XL6_VALUE_ADMIT_U64};
    const int64_t maximum[] = {INT8_MAX, INT16_MAX, INT32_MAX, INT64_MAX, UINT8_MAX, UINT16_MAX, UINT32_MAX, INT64_MAX};
    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); ++i) {
        xvalue* source = xrtValueUInt((uint64_t)maximum[i]); assert(source);
        xvalue* result = xl6PersistenceRestoreScalarV1(source, rules[i], true); assert(result);
        assert(xrtValueType(result) == (rules[i] == XL6_VALUE_ADMIT_U64 ? XVALUE_UINT : XVALUE_INT));
        xrtValueRelease(result); xrtValueRelease(source);
        if (rules[i] != XL6_VALUE_ADMIT_U64) {
            source = xrtValueUInt((uint64_t)maximum[i] + 1); assert(source);
            assert(!xl6PersistenceRestoreScalarV1(source, rules[i], true)); xrtValueRelease(source);
        }
    }
    xvalue* source = xrtValueUInt(UINT64_MAX); assert(source);
    xvalue* result = xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_U64, false); assert(result);
    uint64_t unsignedNumber; assert(xrtValueGetUInt(result, &unsignedNumber) && unsignedNumber == UINT64_MAX);
    xrtValueRelease(result); xrtValueRelease(source);
    source = xrtValueFloat(0.1); assert(source);
    result = xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_F32, false); assert(result);
    double number; assert(xrtValueGetFloat(result, &number) && number == (double)(float)0.1);
    assert(!xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_I64, true));
    xrtValueRelease(result); xrtValueRelease(source);
    source = xrtValueFloat((double)FLT_MAX * 2); assert(source);
    assert(!xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_F32, false)); xrtValueRelease(source);
    source = xrtValueInt(0x10FFFF); assert(source);
    result = xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_CHAR, true); assert(result);
    assert(xrtValueType(result) == XVALUE_CHAR);
    assert(!xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_CHAR, false));
    xrtValueRelease(result); xrtValueRelease(source);
    source = xrtValueInt(0xD800); assert(source);
    assert(!xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_CHAR, true)); xrtValueRelease(source);
    source = xrtValueString((xstrview){"a\0b", 3}); assert(source);
    result = xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_STRING, true); assert(result);
    xstrview text; assert(xrtValueGetString(result, &text) && text.Size == 3 && text.Data[2] == 'b');
    assert(!xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_I8, true));
    xrtValueRelease(result); xrtValueRelease(source);
    assert(!xl6PersistenceRestoreScalarV1(xrtValueNull(), XL6_VALUE_ADMIT_STRING, true));
    assert(!xl6PersistenceRestoreScalarV1(NULL, XL6_VALUE_ADMIT_ANY, true));
    source = xrtValueBytes((xbytesview){(const unsigned char*)"a\0b", 3}); assert(source);
    assert(!xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_BYTES, true));
    result = xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_BYTES, false); assert(result);
    xbytesview data; assert(xrtValueGetBytes(result, &data) && data.Size == 3);
    xrtValueRelease(result); xrtValueRelease(source);
    source = xrtValueTime((xtime)INT64_MIN); assert(source);
    assert(!xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_TIME, true));
    result = xl6PersistenceRestoreScalarV1(source, XL6_VALUE_ADMIT_TIME, false); assert(result);
    xtime instant; assert(xrtValueGetTime(result, &instant) && instant == (xtime)INT64_MIN);
    xrtValueRelease(result); xrtValueRelease(source);
    source = xrtValueArray(); assert(source);
    assert(xrtValueArrayAppendNew(source, xrtValueInt(127)));
    result = xl6PersistenceRestoreCollectionV1(source, XL6_PERSIST_RESTORE_LIST_V1, 42, restoreTiny); assert(result);
    assert(xrtValueType(result) == XVALUE_INT_MAP && xrtValueCount(result) == 1);
    xrtValueRelease(result);
    assert(xrtValueArrayAppendNew(source, xrtValueInt(128)));
    assert(!xl6PersistenceRestoreCollectionV1(source, XL6_PERSIST_RESTORE_LIST_V1, 42, restoreTiny));
    assert(!xl6PersistenceRestoreCollectionV1(source, XL6_PERSIST_RESTORE_DICT_V1, 42, restoreTiny));
    xrtValueRelease(source);
    assert(!xrtGetError());
}

static bool rejectAssignment(xvalue* target, const xvalue* source)
{
    assert(target != NULL && source != NULL); return false;
}
static bool allocateAssignment(xvalue* target, const xvalue* source)
{
    xvalue* item;
    assert(target != NULL && source != NULL);
    item = xrtValueString(xrtStrView("restore-owned-allocation"));
    if (item == NULL) return false;
    xrtValueRelease(item); return true;
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    scalarDomains();
    xvalue* target = xrtValueObject(); assert(target != NULL);
    for (unsigned i = 0; i < 2; ++i) {
        xvalue* output = target;
        xrtSetErrorKind(i ? XERR_MEMORY : XERR_VALUE);
        xerror* prior = xrtTakeError(); assert(prior != NULL);
        assert(xl6SerializeRestoreReadFinishV1(NULL, target, rejectAssignment, prior, &output));
        assert(output != NULL && xrtValueType(output) == XVALUE_NULL && xrtGetError() == prior);
        xrtValueRelease(output); xrtClearError();
    }
    xvalue* source = xrtValueObject(); assert(source != NULL);
    xvalue* output = target;
    assert(xl6SerializeRestoreReadFinishV1(source, target, rejectAssignment, NULL, &output));
    assert(output != NULL && xrtValueType(output) == XVALUE_NULL && xrtGetError() == NULL);
    xrtValueRelease(output);
    source = xrtValueObject(); assert(source != NULL);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6SerializeRestoreReadFinishV1(source, target, allocateAssignment, NULL, &output));
    assert(xrtMemDebugFailTriggered() && output == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear(); xrtClearError();
    source = xrtValueObject(); assert(source != NULL);
    assert(!xl6SerializeRestoreReadFinishV1(source, target, NULL, NULL, &output));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT); xrtClearError();
    xrtValueRelease(target); assert(!xl6ProductMemoryEnd());
    xl6ProductMemoryVerify();
    puts("Typed restore status/error ownership balanced"); return 0;
}
