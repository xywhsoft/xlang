#if XL6_FACTORY_PROFILE == 7
#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#if XL6_FACTORY_PROFILE == 8
#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#endif
#define main xl6MethodsFactoryFixtureMain
#include "test/cases/stdlib_sequence_factory_only_host.c"
#undef main
#include "src/runtime/xl6_array_methods.h"
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
static void vectors(bool native)
{
    xvalue* good=item(),*value=prepared(native,good); assert(good);
    assert(xl6ArrayMethodCapacityV1(value)>=2 && xl6ArrayMethodReserveV1(value,32));
    assert(xl6ArrayMethodIndexOfV1(value,good)==0);
    assert(xl6ArrayMethodInsertV1(value,1,good) && size(value,native)==3);
    assert(xl6ArrayMethodSwapV1(value,0,2) && xl6ArrayMethodReverseV1(value));
    assert(xl6ArrayMethodRemoveValueV1(value,good) && size(value,native)==2);
    assert(xl6ArrayMethodTrimV1(value));
    assert(!xl6ArrayMethodReserveV1(value,-1)); error(XERR_RANGE);
    assert(!xl6ArrayMethodInsertV1(value,-1,good)); error(XERR_RANGE);
    assert(!xl6ArrayMethodSwapV1(value,0,3)); error(XERR_RANGE);
    if (native) {
        const xrttype* type; unsigned transfer; xvalue* tuple=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer); assert(tuple); xrtValueRelease(tuple);
        XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,1001,3001,transfer,type->Size,type);
        assert(pin && !xl6ArrayMethodClearV1(value)); error(XERR_STATE);
        assert(xl6ArrayMethodCapacityV1(value)==0); error(XERR_STATE);
        assert(xl6ArrayMethodIndexOfV1(value,good)==-1); error(XERR_STATE);
        xl6SequenceEndV1(pin);
    }
    assert(xl6ArrayMethodClearV1(value) && !size(value,native));
    assert(xl6ArrayMethodIndexOfV1(value,good)==-1 && !xrtGetError());
    assert(!xl6ArrayMethodRemoveValueV1(value,good) && !xrtGetError());
    assert(xl6ArrayMethodReverseV1(value) && xl6ArrayMethodTrimV1(value));
    assert(xl6ArrayMethodCapacityV1(value)==0);
    xrtValueRelease(value); xrtValueRelease(good);
}
static bool operation(xvalue* value,const xvalue* good,const char* op)
{
    if (!strcmp(op,"reserve")) return xl6ArrayMethodReserveV1(value,65);
    if (!strcmp(op,"insert")) return xl6ArrayMethodInsertV1(value,1,good);
    if (!strcmp(op,"find")) return xl6ArrayMethodIndexOfV1(value,good)==0;
    if (!strcmp(op,"remove")) return xl6ArrayMethodRemoveValueV1(value,good);
    if (!strcmp(op,"swap")) return xl6ArrayMethodSwapV1(value,0,1);
    if (!strcmp(op,"reverse")) return xl6ArrayMethodReverseV1(value);
    assert(!strcmp(op,"trim")); return xl6ArrayMethodTrimV1(value);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    if (argc==4) {
        bool native=!strcmp(argv[1],"native"); xvalue* good=item(),*value=prepared(native,good);
        assert(xrtMemDebugFailAfter((size_t)strtoull(argv[3],NULL,10)));
        bool ok=operation(value,good,argv[2]),hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (hit) { assert(!ok && size(value,native)==2); error(XERR_MEMORY); }
        else assert(ok && !xrtGetError());
        xrtValueRelease(value); xrtValueRelease(good); balanced(&before);
        printf("array-method profile=%u %s %s point=%s hit=%u balanced\n",XL6_FACTORY_PROFILE,argv[1],argv[2],argv[3],hit); return 0;
    }
    assert(argc==1);
    for (unsigned cycle=0;cycle<100;++cycle) { vectors(false); vectors(true); }
    assert(!xl6ArrayMethodClearV1(NULL)); error(XERR_ARGUMENT);
    assert(xl6ArrayMethodCapacityV1(NULL)==0); error(XERR_ARGUMENT);
    assert(!xl6ArrayMethodReserveV1(NULL,1)); error(XERR_ARGUMENT);
    assert(!xl6ArrayMethodTrimV1(NULL)); error(XERR_ARGUMENT);
    assert(!xl6ArrayMethodSwapV1(NULL,0,0)); error(XERR_ARGUMENT);
    assert(!xl6ArrayMethodReverseV1(NULL)); error(XERR_ARGUMENT);
    xvalue* good=item(); assert(good);
    assert(!xl6ArrayMethodInsertV1(NULL,0,good)); error(XERR_ARGUMENT);
    assert(xl6ArrayMethodIndexOfV1(NULL,good)==-1); error(XERR_ARGUMENT);
    assert(!xl6ArrayMethodRemoveValueV1(NULL,good)); error(XERR_ARGUMENT);
    const xrttype* type; unsigned transfer;
    xvalue* unbound=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer); assert(unbound);
    assert(xl6ArrayMethodCapacityV1(unbound)==0); error(XERR_STATE);
    assert(!xl6ArrayMethodInsertV1(unbound,0,good)); error(XERR_STATE);
    assert(xl6ArrayMethodIndexOfV1(unbound,good)==-1); error(XERR_STATE);
    xrtValueRelease(unbound);
    xvalue* stack=factory(XL6_SEQUENCE_STACK_V1,&type,&transfer); assert(stack);
    assert(!xl6ArrayMethodClearV1(stack)); error(XERR_TYPE);
    assert(!xl6ArrayMethodInsertV1(stack,0,good)); error(XERR_TYPE);
    assert(xl6ArrayMethodIndexOfV1(stack,good)==-1); error(XERR_TYPE);
    xrtValueRelease(stack); xrtValueRelease(good);
    xvalue* wrong=xrtValueInt(1); assert(xl6ArrayMethodIndexOfV1(wrong,wrong)==-1); error(XERR_TYPE); xrtValueRelease(wrong);
    balanced(&before); printf("Array methods profile=%u: 100 native/wire cycles balanced\n",XL6_FACTORY_PROFILE); return 0;
}
