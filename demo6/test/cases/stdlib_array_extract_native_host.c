#if XL6_FACTORY_PROFILE == 7
#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#if XL6_FACTORY_PROFILE == 8
#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#endif
#define main xl6ExtractFactoryFixtureMain
#include "test/cases/stdlib_sequence_factory_only_host.c"
#undef main
#include "src/runtime/xl6_array_extract.h"
#include "src/runtime/xl6_array_erased.h"
#include <stdlib.h>
#include <string.h>

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
#if XL6_FACTORY_PROFILE == 7
static bool entry(ptr context,const xrtcallframe* frame,xrtcallresult* result)
{ assert(!context && frame && result); return true; }
#endif
static xvalue* item(void)
{
#if XL6_FACTORY_PROFILE == 0
    return xrtValueString((xstrview){"a\0你",5});
#elif XL6_FACTORY_PROFILE == 4
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,3001,4001,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,4001);
#elif XL6_FACTORY_PROFILE == 7
    xrtcallable* callable=xrtCallableCreate(NULL,entry,NULL,NULL); assert(callable);
    xvalue* value=xrtValueCallableTake(&callable); assert(value && xrtValueTypeIdBind(value,3001)); return value;
#elif XL6_FACTORY_PROFILE == 8
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
    assert(xrtPromiseResolve(promise,NULL)); xrtPromiseDestroy(promise);
    xvalue* value=xrtValueFuture(future); xrtFutureDestroy(future);
    assert(value && xrtValueTypeIdBind(value,3001)); return value;
#else
    xvalue* value;
#if XL6_FACTORY_PROFILE == 3 || XL6_FACTORY_PROFILE == 5
    value=xrtValueUInt(65535);
#else
    value=xrtValueInt(65);
#endif
    assert(value);
#if XL6_FACTORY_PROFILE == 1 || XL6_FACTORY_PROFILE == 6
    assert(xrtValueTypeIdBind(value,3001));
#endif
    return value;
#endif
}
static xvalue* prepared(bool native,const xvalue* good)
{
    if (!native) {
        xvalue* value=xrtValueArray(); assert(value && xrtValueArrayAppend(value,good) && xrtValueArrayAppend(value,good));
        return value;
    }
    const xrttype* type; unsigned transfer;
    xvalue* value=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer); assert(value);
    const xvalueconverter* converter=NULL;
#if XL6_FACTORY_PROFILE == 5
    converter=xl6NullableNativeU16ConverterV1();
#endif
    xvalue* alias=xl6ArrayEraseV1(value,1001,3001,transfer,type->Size,type,converter);
    assert(alias==value); xrtValueRelease(alias);
    assert(xl6ArrayErasedPushV1(value,good) && xl6ArrayErasedPushV1(value,good)); return value;
}
static size_t size(const xvalue* value,bool native)
{ return native ? (size_t)xl6ArrayErasedSizeV1(value) : xrtValueCount(value); }
static void projectionRefusal(void)
{
#if XL6_FACTORY_PROFILE == 1 || XL6_FACTORY_PROFILE == 2 || XL6_FACTORY_PROFILE == 4 || XL6_FACTORY_PROFILE == 6 || XL6_FACTORY_PROFILE == 7
    xvalue* good=item(),*value=prepared(true,good); const xrttype* type; unsigned transfer;
    xvalue* temporary=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer); assert(temporary); xrtValueRelease(temporary);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,1001,3001,transfer,type->Size,type); assert(pin);
#if XL6_FACTORY_PROFILE == 2 || XL6_FACTORY_PROFILE == 6
    int64_t invalid=128; assert(xrtTypedArraySet(xl6SequenceStorageV1(pin),0,&invalid));
#else
    xvalue* invalid=
#if XL6_FACTORY_PROFILE == 4
        xrtValueArray();
#else
        xrtValueInt(1);
#endif
    assert(invalid && xrtTypedArraySet(xl6SequenceStorageV1(pin),0,&invalid)); xrtValueRelease(invalid);
#endif
    xl6SequenceEndV1(pin);
    assert(!xl6ArrayExtractValueV1(value,1001,3001,0,false)); error(XERR_TYPE);
    assert(size(value,true)==2); /* admission precedes destructive removal */
    xrtValueRelease(value); xrtValueRelease(good);
#endif
}
static void vectors(bool native)
{
    xvalue* good=item(),*value=prepared(native,good),*alias=xrtValueRetain(value); assert(good && alias);
    if (native) {
        assert(!xl6ArrayExtractValueV1(value,1002,3001,0,false)); error(XERR_TYPE);
        assert(!xl6ArrayExtractValueV1(value,1001,3002,0,false)); error(XERR_TYPE);
        const xrttype* type; unsigned transfer; xvalue* temporary=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer);
        assert(temporary); xrtValueRelease(temporary);
        XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,1001,3001,transfer,type->Size,type);
        assert(pin && !xl6ArrayExtractValueV1(value,1001,3001,0,true)); error(XERR_STATE);
        xl6SequenceEndV1(pin);
    }
    assert(!xl6ArrayExtractValueV1(value,1001,3001,-1,false)); error(XERR_RANGE);
    assert(!xl6ArrayExtractValueV1(value,1001,3001,INT64_MAX,false)); error(XERR_RANGE);
    assert(!xl6ArrayExtractValueV1(value,1001,3001,1,true)); error(XERR_ARGUMENT);
    assert(!xl6ArrayExtractValueV1(value,1001,3001,2,false)); error(XERR_RANGE);
    assert(size(value,native)==2);
    xvalue* result=xl6ArrayExtractValueV1(value,1001,3001,0,false);
    assert(result && xrtValueEqual(result,good) && size(alias,native)==1 && !xrtGetError()); xrtValueRelease(result);
    result=xl6ArrayExtractValueV1(value,1001,3001,0,true);
    assert(result && xrtValueEqual(result,good) && !size(alias,native) && !xrtGetError()); xrtValueRelease(result);
    assert(!xl6ArrayExtractValueV1(value,1001,3001,0,true)); error(XERR_RANGE);
    xrtValueRelease(alias); xrtValueRelease(value); xrtValueRelease(good);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    if (argc==4) {
        bool native=!strcmp(argv[1],"native"),pop=!strcmp(argv[2],"pop");
        assert(pop || !strcmp(argv[2],"take")); xvalue* good=item(),*value=prepared(native,good);
        xvalue* snapshot=native ? NULL : xrtValueClone(value); assert(native || snapshot);
        assert(xrtMemDebugFailAfter((size_t)strtoull(argv[3],NULL,10)));
        xvalue* result=xl6ArrayExtractValueV1(value,1001,3001,0,pop);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (hit) { assert(!result && size(value,native)==2); error(XERR_MEMORY); }
        else { assert(result && xrtValueEqual(result,good) && size(value,native)==1 && !xrtGetError()); xrtValueRelease(result); }
        if(snapshot) assert(xrtValueCount(snapshot)==2);
        xrtValueRelease(snapshot); xrtValueRelease(value); xrtValueRelease(good); balanced(&before);
        printf("array-extract profile=%u %s %s point=%s hit=%u balanced\n",XL6_FACTORY_PROFILE,argv[1],argv[2],argv[3],hit); return 0;
    }
    assert(argc==1);
    for (unsigned cycle=0;cycle<100;++cycle) { vectors(false); vectors(true); }
    projectionRefusal();
    assert(!xl6ArrayExtractValueV1(NULL,1001,3001,0,true)); error(XERR_ARGUMENT);
    xvalue* wrong=xrtValueInt(1); assert(!xl6ArrayExtractValueV1(wrong,1001,3001,0,true)); error(XERR_TYPE); xrtValueRelease(wrong);
    const xrttype* type; unsigned transfer;
    xvalue* unbound=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer); assert(unbound);
    assert(!xl6ArrayExtractValueV1(unbound,1001,3001,0,true)); error(XERR_STATE); xrtValueRelease(unbound);
    xvalue* stack=factory(XL6_SEQUENCE_STACK_V1,&type,&transfer); assert(stack);
    assert(!xl6ArrayExtractValueV1(stack,1001,3001,0,true)); error(XERR_TYPE); xrtValueRelease(stack);
    balanced(&before); printf("Array extraction profile=%u: 100 native/wire cycles balanced\n",XL6_FACTORY_PROFILE); return 0;
}
