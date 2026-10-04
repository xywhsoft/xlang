#include "src/runtime/xl6_value_admission.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static void probe(xvalue* value, unsigned int rule, bool expected)
{
    assert(value); xrtSetErrorKind(XERR_RANGE); const xerror* error=xrtGetError();
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ValueAdmitsV1(value,rule,false,0)==expected && xrtGetError()==error);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    xrtValueRelease(value); assert(xrtGetError()==error); xrtClearError();
}
static void drop(void* value, void* context)
{ (void)value; ++*(unsigned int*)context; xrtSetErrorKind(XERR_RANGE); }
static void vectors(void)
{
    const int64_t limits[]={INT8_MAX,INT16_MAX,INT32_MAX,INT64_MAX};
    const unsigned int signedRules[]={XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I16,XL6_VALUE_ADMIT_I32,XL6_VALUE_ADMIT_I64};
    const uint64_t maxima[]={UINT8_MAX,UINT16_MAX,UINT32_MAX,UINT64_MAX};
    for (unsigned int i=0;i<4;++i) {
        probe(xrtValueInt(limits[i]),signedRules[i],true);
        probe(xrtValueInt(-limits[i]-1),signedRules[i],true);
        probe(xrtValueUInt((uint64_t)limits[i]),signedRules[i],false);
        if(i<3) { probe(xrtValueInt(limits[i]+1),signedRules[i],false); probe(xrtValueInt(-limits[i]-2),signedRules[i],false); }
        if(i<3) {
            probe(xrtValueInt((int64_t)maxima[i]),XL6_VALUE_ADMIT_U8+i,true);
            probe(xrtValueInt(-1),XL6_VALUE_ADMIT_U8+i,false);
            probe(xrtValueInt((int64_t)maxima[i]+1),XL6_VALUE_ADMIT_U8+i,false);
            probe(xrtValueUInt(0),XL6_VALUE_ADMIT_U8+i,false);
        } else { probe(xrtValueUInt(UINT64_MAX),XL6_VALUE_ADMIT_U64,true); probe(xrtValueInt(0),XL6_VALUE_ADMIT_U64,false); }
    }
    probe(xrtValueFloat(1.5),XL6_VALUE_ADMIT_F32,true);
    probe(xrtValueFloat(0.1),XL6_VALUE_ADMIT_F32,false);
    probe(xrtValueFloat(DBL_MAX),XL6_VALUE_ADMIT_F32,false);
    probe(xrtValueFloat((double)FLT_MAX),XL6_VALUE_ADMIT_F32,true);
    probe(xrtValueFloat((double)FLT_TRUE_MIN),XL6_VALUE_ADMIT_F32,true);
    probe(xrtValueFloat(DBL_TRUE_MIN),XL6_VALUE_ADMIT_F32,false);
    probe(xrtValueFloat(-0.0),XL6_VALUE_ADMIT_F32,true);
    probe(xrtValueFloat(INFINITY),XL6_VALUE_ADMIT_F32,true);
    probe(xrtValueFloat(NAN),XL6_VALUE_ADMIT_F32,true);
    probe(xrtValueInt(1),XL6_VALUE_ADMIT_F64,false);
    probe(xrtValueChar(0x4f60),XL6_VALUE_ADMIT_CHAR,true);
    probe(xrtValueUInt(0x4f60),XL6_VALUE_ADMIT_CHAR,false);
    probe(xrtValueString((xstrview){"a\0b",3}),XL6_VALUE_ADMIT_STRING,true);
    probe(xrtValueString((xstrview){"",0}),XL6_VALUE_ADMIT_BYTES,false);
    probe(xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}),XL6_VALUE_ADMIT_BYTES,true);
    probe(xrtValueTime(17),XL6_VALUE_ADMIT_TIME,true);
    probe(xrtValueInt(17),XL6_VALUE_ADMIT_TIME,false);
    probe(xrtValueBool(true),XL6_VALUE_ADMIT_BOOL,true);
    probe(xrtValueNull(),XL6_VALUE_ADMIT_ANY,true);
    for (unsigned int rule=XL6_VALUE_ADMIT_BOOL;rule<=XL6_VALUE_ADMIT_TIME;++rule) {
        assert(!xl6ValueAdmitsV1(xrtValueNull(),rule,false,0));
        assert(xl6ValueAdmitsV1(xrtValueNull(),rule,true,0));
    }
    assert(!xl6ValueAdmitsV1(NULL,XL6_VALUE_ADMIT_ANY,true,0));
    assert(!xl6ValueAdmitsV1(xrtValueNull(),999,true,0));
    assert(!xl6ValueAdmitsV1(xrtValueNull(),XL6_VALUE_ADMIT_NONE,true,0));
    float output=19.0f; xvalue* real=xrtValueFloat(0.1); assert(real);
    assert(!xl6ValueReadF32V1(real,0,&output) && output==19.0f && xrtErrorKind(xrtGetError())==XERR_TYPE);
    xrtClearError(); xrtValueRelease(real);
    real=xrtValueFloat(-0.0); assert(real && xl6ValueReadF32V1(real,0,&output) && signbit(output)); xrtValueRelease(real);
    xvalue* bytes=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(bytes);
    assert(xl6ValueCheckedRetainV1(bytes,XL6_VALUE_ADMIT_BYTES,false,0)==bytes);
    xrtValueRelease(bytes); xrtValueRelease(bytes);
    bytes=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(bytes && xrtValueTypeIdBind(bytes,101));
    assert(!xl6ValueAdmitsV1(bytes,XL6_VALUE_ADMIT_BYTES,false,102));
    assert(xl6ValueAdmitsV1(bytes,XL6_VALUE_ADMIT_BYTES,false,101));
    xvalue* boundArray=xrtValueArray(); assert(boundArray && xrtValueArrayAppendNew(boundArray,bytes));
    assert(!xl6CollectionCheckedTakeV1(boundArray,XVALUE_ARRAY,XL6_VALUE_ADMIT_BYTES,false,102));
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    unsigned int drops=0; void* handle=&drops;
    const xvaluehandleops ops={.Drop=drop};
    xvalue* wrong=xrtValueHandleTake(&handle,&ops,&drops); assert(wrong);
    assert(!xl6CollectionCheckedTakeV1(wrong,XVALUE_ARRAY,XL6_VALUE_ADMIT_I64,false,0));
    assert(drops==1 && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();

    xvalue* array=xrtValueArray(); assert(array && xrtValueArrayAppendNew(array,xrtValueInt(127)));
    xvalue* alias=xrtValueRetain(array); assert(alias);
    assert(xl6CollectionCheckedTakeV1(array,XVALUE_ARRAY,XL6_VALUE_ADMIT_I8,false,0)==alias);
    xrtValueRelease(array);
    assert(xrtValueArrayAppendNew(alias,xrtValueInt(128)));
    assert(!xl6CollectionCheckedTakeV1(xrtValueRetain(alias),XVALUE_ARRAY,XL6_VALUE_ADMIT_I8,false,0));
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    assert(xrtValueCount(alias)==2 && xrtValueType(xrtValueArrayAt(alias,1))==XVALUE_INT);
    xrtValueRelease(alias);
    array=xrtValueArray(); assert(array && xrtValueArrayAppendNew(array,xrtValueNull()));
    assert(xl6CollectionCheckedTakeV1(array,XVALUE_ARRAY,XL6_VALUE_ADMIT_STRING,true,0)==array); xrtValueRelease(array);
    array=xrtValueArray(); xvalue* weak=xrtValueWeakRef(array); assert(weak);
    assert(!xl6CollectionCheckedTakeV1(array,XVALUE_ARRAY,999,false,0));
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT && xrtValueWeakRefExpired(weak));
    xrtClearError(); xrtValueRelease(weak);
    assert(!xl6CollectionCheckedTakeV1(xrtValueInt(1),XVALUE_ARRAY,XL6_VALUE_ADMIT_I64,false,0));
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
}
static void fault(const char* mode, size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* array=xrtValueArray(); assert(array);
    assert(xrtValueArrayAppendNew(array,xrtValueInt(127)));
    if(!strcmp(mode,"mismatch")) assert(xrtValueArrayAppendNew(array,xrtValueInt(128)));
    else assert(!strcmp(mode,"validate"));
    xvalue* alias=xrtValueRetain(array); assert(alias && xrtMemDebugFailAfter(point));
    xvalue* result=xl6CollectionCheckedTakeV1(array,XVALUE_ARRAY,XL6_VALUE_ADMIT_I8,false,0);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if(hit) assert(!result && xrtErrorKind(xrtGetError())==XERR_MEMORY);
    else if(!strcmp(mode,"mismatch")) assert(!result && xrtErrorKind(xrtGetError())==XERR_TYPE);
    else assert(result==alias && !xrtGetError());
    xrtClearError(); assert(xrtValueCount(alias)==(!strcmp(mode,"mismatch") ? 2u : 1u));
    xrtValueRelease(result); xrtValueRelease(alias); balanced(&before);
    printf("value-admission %s point=%zu hit=%d balanced\n",mode,point,hit);
}
int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true));
    if(argc==3) { fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    vectors(); balanced(&before); puts("value-admission vectors balanced"); return 0;
}
