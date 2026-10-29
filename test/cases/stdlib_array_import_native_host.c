#if XL6_FACTORY_PROFILE == 7
#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#if XL6_FACTORY_PROFILE == 8
#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#endif
/* Reuse the nine real resident factories, not a test-specific schema oracle. */
#define main xl6ImportFactoryFixtureMain
#include "test/cases/stdlib_sequence_factory_only_host.c"
#undef main
#include "src/runtime/xl6_array_import.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_reference_value.h"
#include <stdlib.h>
#include <string.h>

static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static xvalue* seed(bool bind)
{
    const xrttype* type; unsigned int transfer;
    xvalue* value=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer);
    if (!value || !bind) return value;
    const xvalueconverter* converter=NULL;
#if XL6_FACTORY_PROFILE == 5
    converter=xl6NullableNativeU16ConverterV1();
#endif
    xvalue* alias=xl6ArrayEraseV1(value,1001,3001,transfer,type->Size,type,converter);
    assert(alias==value); xrtValueRelease(alias); return value;
}
#if XL6_FACTORY_PROFILE == 7
static unsigned invoked;
static bool entry(ptr context,const xrtcallframe* frame,xrtcallresult* result)
{ assert(!context && frame && result); ++invoked; return true; }
#endif
static xvalue* item(bool bad)
{
#if XL6_FACTORY_PROFILE == 0
    (void)bad; return xrtValueString((xstrview){"a\0你",5});
#elif XL6_FACTORY_PROFILE == 4
    if (bad) { xvalue* fake=xrtValueArray(); assert(fake && xrtValueTypeIdBind(fake,3001)); return fake; }
    xvalue* child=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,3001,4001,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I8,false,4001);
    assert(child); return child;
#elif XL6_FACTORY_PROFILE == 7
    if (bad) { xvalue* fake=xrtValueUInt(65); assert(fake && xrtValueTypeIdBind(fake,3001)); return fake; }
    xrtcallable* callable=xrtCallableCreate(NULL,entry,NULL,NULL); assert(callable);
    xvalue* value=xrtValueCallableTake(&callable); assert(value && !callable && xrtValueTypeIdBind(value,3001)); return value;
#elif XL6_FACTORY_PROFILE == 8
    if (bad) { xvalue* fake=xrtValueUInt(65); assert(fake && xrtValueTypeIdBind(fake,3001)); return fake; }
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
    assert(xrtPromiseResolve(promise,NULL)); xrtPromiseDestroy(promise);
    xvalue* value=xrtValueFuture(future); xrtFutureDestroy(future);
    assert(value && xrtValueTypeIdBind(value,3001)); return value;
#else
    xvalue* value;
#if XL6_FACTORY_PROFILE == 3 || XL6_FACTORY_PROFILE == 5
    value=xrtValueUInt(bad ? UINT64_MAX : UINT64_C(65535));
#else
    value=xrtValueInt(bad ? 128 : 65);
#endif
    assert(value);
#if XL6_FACTORY_PROFILE == 1 || XL6_FACTORY_PROFILE == 6
    assert(xrtValueTypeIdBind(value,bad ? 3002 : 3001));
#endif
    return value;
#endif
}
static xvalue* wire(bool bad)
{
    xvalue* source=xrtValueArray(); xvalue* good=item(false); assert(source && good);
    assert(xrtValueArrayAppend(source,good) && xrtValueArrayAppend(source,good)); xrtValueRelease(good);
    if (bad) assert(xrtValueArrayAppendNew(source,item(true)));
    return source;
}
static void observed(const xvalue* source)
{
    assert(xl6ArrayErasedSizeV1(source)==2);
    xvalue* first=xl6ArrayErasedReadV1(source,0),*second=xl6ArrayErasedReadV1(source,1); assert(first && second);
#if XL6_FACTORY_PROFILE == 0
    xstrview text; assert(xrtValueGetString(first,&text) && text.Size==5 && !memcmp(text.Data,"a\0你",5));
#elif XL6_FACTORY_PROFILE == 4
    assert(first==second && xl6SequenceAdmitsArrayV1(first,3001));
#elif XL6_FACTORY_PROFILE == 7
    assert(first==second && xrtValueIsCallable(first));
    xrtcallframe frame={0}; xrtcallresult result=XRT_CALL_RESULT_INIT;
    assert(xrtCallableInvoke(xrtValueGetCallable(first),&frame,&result)); xrtCallResultClear(&result);
#elif XL6_FACTORY_PROFILE == 8
    assert(xrtValueIsFuture(first) && xrtValueGetFuture(first)==xrtValueGetFuture(second));
#else
    assert(xrtValueEqual(first,second));
#endif
    xrtValueRelease(first); xrtValueRelease(second);
}
static void vectors(void)
{
    xvalue* source=wire(false),*destination=seed(true); assert(destination);
    xvalue* same=destination;
    assert(xl6ArrayImportTakeV1(destination,source)==same && xrtValueCount(source)==2);
    observed(same);
    xvalue* second=xl6ArrayImportTakeV1(seed(true),same); assert(second); observed(second);
    xrtValueRelease(same); observed(second); xrtValueRelease(second);
    assert(!xl6ArrayImportTakeV1(source,source)); error(XERR_ARGUMENT); /* Consumed seed. */
    source=wire(false);
    assert(!xl6ArrayImportTakeV1(seed(false),source)); error(XERR_STATE);
    assert(!xl6ArrayImportTakeV1(xrtValueArray(),source)); error(XERR_TYPE);
    destination=seed(true); assert(destination);
    xvalue* good=item(false); assert(xl6ArrayErasedPushV1(destination,good)); xrtValueRelease(good);
    assert(!xl6ArrayImportTakeV1(destination,source)); error(XERR_STATE);
    destination=seed(true); assert(destination);
    const xrttype* type; unsigned int transfer;
    xvalue* tuple=factory(XL6_SEQUENCE_ARRAY_V1,&type,&transfer); assert(tuple); xrtValueRelease(tuple);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(destination,XL6_SEQUENCE_ARRAY_V1,1001,3001,transfer,type->Size,type);
    assert(pin && !xl6ArrayImportTakeV1(xrtValueRetain(destination),source)); error(XERR_STATE);
    xl6SequenceEndV1(pin); xrtValueRelease(destination);
#if XL6_FACTORY_PROFILE != 0
    xvalue* bad=wire(true); uint64_t identity=xrtValueTypeId(bad);
    assert(!xl6ArrayImportTakeV1(seed(true),bad)); error(XERR_TYPE);
    assert(xrtValueCount(bad)==3 && xrtValueTypeId(bad)==identity); xrtValueRelease(bad);
#endif
#if XL6_FACTORY_PROFILE == 4
    /* Identical IDs and physical slots are not a logical schema witness:
     * an I64 child must not enter the I8 child domain, through either path. */
    xvalue* wide=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,3001,4001,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,4001);
    assert(wide);
    xvalue* wrongWire=xrtValueArray(); assert(wrongWire);
    assert(xrtValueArrayAppend(wrongWire,wide) && xrtValueArrayAppend(wrongWire,wide));
    assert(!xl6ArrayImportTakeV1(seed(true),wrongWire)); error(XERR_TYPE);
    assert(xrtValueCount(wrongWire)==2 && xrtValueArrayGet(wrongWire,0)==wide);
    xvalue* wrongNative=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,1001,3001,
        xrtValueRetain(wide),false);
    assert(wrongNative);
    xvalue* projection=xl6ArrayEraseV1(wrongNative,1001,3001,3,xrtTypeValue()->Size,xrtTypeValue(),NULL);
    assert(projection==wrongNative); xrtValueRelease(projection);
    assert(xl6ArrayErasedPushV1(wrongNative,wide));
    assert(!xl6ArrayImportTakeV1(seed(true),wrongNative)); error(XERR_TYPE);
    assert(xl6ArrayErasedSizeV1(wrongNative)==1);
    xvalue* preserved=xl6ArrayErasedReadV1(wrongNative,0); assert(preserved==wide); xrtValueRelease(preserved);
    xrtValueRelease(wrongNative); xrtValueRelease(wrongWire); xrtValueRelease(wide);
#endif
    destination=seed(true); assert(destination);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError();
    same=xl6ArrayImportTakeV1(destination,source); assert(same && xrtGetError()==prior); xrtClearError();
    xrtValueRelease(same); xrtValueRelease(source);
}
static void fault(bool native,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* source=wire(false);
    if (native) { xvalue* prepared=xl6ArrayImportTakeV1(seed(true),source); assert(prepared); xrtValueRelease(source); source=prepared; }
    xvalue* destination=seed(true); assert(destination);
    assert(xrtMemDebugFailAfter(point)); xvalue* result=xl6ArrayImportTakeV1(destination,source);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); }
    else { assert(result && !xrtGetError()); observed(result); }
    if (native) observed(source); else assert(xrtValueCount(source)==2);
    xrtValueRelease(result); xrtValueRelease(source); balanced(&before);
    printf("array-import profile=%u native=%u point=%zu hit=%u balanced\n",XL6_FACTORY_PROFILE,native,point,hit);
}
#ifndef XL6_IMPORT_FIXTURE_MAIN
#define XL6_IMPORT_FIXTURE_MAIN main
#endif
int XL6_IMPORT_FIXTURE_MAIN(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) { fault(!strcmp(argv[1],"native"),(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (unsigned cycle=0;cycle<100;++cycle) vectors();
    balanced(&before); printf("array-import profile=%u: 100 source/destination cycles balanced\n",XL6_FACTORY_PROFILE); return 0;
}
