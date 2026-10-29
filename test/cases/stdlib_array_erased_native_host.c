#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_array_iterator.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_nullable_string.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define I64_ARGS UINT64_C(1001),UINT64_C(2001),1,sizeof(int64_t),xrtTypeInt64()
#define TEXT_ARGS UINT64_C(1002),UINT64_C(2002),2,sizeof(xstrview),xl6StringTypeV1()
#define REF_ARGS UINT64_C(1003),UINT64_C(2003),3,sizeof(xvalue*),xrtTypeValue()
#define CHAR_ARGS UINT64_C(1004),UINT64_C(2004),1,sizeof(uint32_t),xl6CharTypeV1()
#define OPTIONAL_ARGS UINT64_C(1005),UINT64_C(2005),2,sizeof(xstrview),xl6NullableStringTypeV1()
static void error(xerrkind kind) { assert(xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static void integer(const xvalue* value, int64_t expected)
{ int64_t actual=0; assert(value && xrtValueGetInt(value,&actual) && actual==expected); }
static void text(const xvalue* value, const char* expected, size_t size)
{ xstrview actual; assert(value && xrtValueGetString(value,&actual) && actual.Data && actual.Size==size && !memcmp(actual.Data,expected,size)); }
static void indexVectors(void)
{
    xvalue* value=xl6ArrayCreateV1(I64_ARGS); assert(value);
    for(int64_t i=0;i<3;++i) assert(xl6ArrayPushV1(value,I64_ARGS,&i));
    int64_t observed=77,item=9; xl6ArrayReadV1(value,I64_ARGS,-1,&observed); assert(observed==2);
    assert(xl6ArraySetV1(value,I64_ARGS,-3,&item));
    assert(xl6ArrayInsertV1(value,I64_ARGS,-1,&item));
    assert(xl6ArraySwapV1(value,I64_ARGS,-4,-1));
    xl6ArrayTakeV1(value,I64_ARGS,-2,&observed); assert(observed==9);
    assert(xl6ArrayRemoveV1(value,I64_ARGS,-1,1));
    observed=73; xl6ArrayReadV1(value,I64_ARGS,INT64_MIN,&observed); assert(observed==73); error(XERR_RANGE);
    xl6ArrayTakeV1(value,I64_ARGS,-3,&observed); assert(observed==73); error(XERR_RANGE);
    assert(!xl6ArraySetV1(value,I64_ARGS,2,&item)); error(XERR_RANGE);
    assert(!xl6ArrayInsertV1(value,I64_ARGS,INT64_MIN,&item)); error(XERR_RANGE);
    assert(!xl6ArrayRemoveV1(value,I64_ARGS,-2,3)); error(XERR_RANGE);
    assert(!xl6ArrayRemoveV1(value,I64_ARGS,0,-1)); error(XERR_RANGE);
    assert(xl6ArraySizeV1(value,I64_ARGS,false)==2); xrtValueRelease(value);
}
static void aliasVectors(void)
{
    xvalue* value=xl6ArrayCreateV1(I64_ARGS); assert(value); int64_t item=11;
    assert(xl6ArrayPushV1(value,I64_ARGS,&item));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    xvalue* alias=xl6ArrayEraseV1(value,I64_ARGS,NULL);
    assert(alias==value && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    xrtMemDebugSnapshot(&after); assert(before.AllocCount==after.AllocCount && before.LiveBytes==after.LiveBytes);
    item=22; assert(xl6ArraySetV1(value,I64_ARGS,0,&item));
    xvalue* projected=xl6ArrayErasedReadV1(alias,-1); integer(projected,22); xrtValueRelease(projected);
    xvalue* replacement=xrtValueInt(33); assert(replacement && xl6ArrayErasedSetV1(alias,0,replacement));
    int64_t output=0; xl6ArrayReadV1(value,I64_ARGS,0,&output); assert(output==33);
    assert(xl6ArrayErasedPushV1(alias,replacement) && xl6ArrayErasedInsertV1(alias,-1,replacement));
    assert(xl6ArraySizeV1(value,I64_ARGS,false)==3);
    xvalue* copy=xl6ArrayErasedCloneV1(alias); assert(copy && copy!=alias && xl6ArrayErasedSizeV1(copy)==3);
    xl6ArrayErasedClearV1(copy); assert(xl6ArrayErasedSizeV1(copy)==0 && xl6ArrayErasedSizeV1(value)==3);
    projected=xl6ArrayErasedTakeV1(alias,-1); integer(projected,33); xrtValueRelease(projected);
    assert(xl6ArraySizeV1(value,I64_ARGS,false)==2);
    assert(!xl6ArrayErasedReadV1(alias,INT64_MIN)); error(XERR_RANGE);
    assert(!xl6ArrayErasedTakeV1(alias,INT64_MAX)); error(XERR_RANGE);
    assert(!xl6ArrayErasedInsertV1(alias,3,replacement)); error(XERR_RANGE);
    assert(xl6ArrayErasedSizeV1(alias)==2);
    xrtValueRelease(value); assert(xl6ArrayErasedSizeV1(alias)==2);
    xrtValueRelease(alias); xrtValueRelease(copy); xrtValueRelease(replacement);
}
static void precisionVectors(void)
{
    xvalue* value=xl6ArrayCreateV1(TEXT_ARGS),*alias=xl6ArrayEraseV1(value,TEXT_ARGS,xl6StringConverterV1());
    xvalue* item=xrtValueString((xstrview){"a\0b",3}); assert(value && alias==value && item);
    assert(xl6ArrayErasedPushV1(alias,item));
    xvalue* output=xl6ArrayErasedReadV1(value,-1); text(output,"a\0b",3); xrtValueRelease(output);
    assert(!xl6ArrayErasedSetV1(alias,0,xrtValueNull())); error(XERR_TYPE);
    output=xl6ArrayErasedTakeV1(alias,-1); text(output,"a\0b",3); xrtValueRelease(output);
    assert(xl6ArrayErasedSizeV1(value)==0); xrtValueRelease(alias); xrtValueRelease(value); xrtValueRelease(item);
    value=xl6ArrayCreateV1(CHAR_ARGS); alias=xl6ArrayEraseV1(value,CHAR_ARGS,xl6CharConverterV1()); assert(alias);
    item=xrtValueChar(0x4f60); assert(item && xl6ArrayErasedPushV1(alias,item));
    output=xl6ArrayErasedReadV1(alias,0); uint32_t scalar=0;
    assert(output && xrtValueType(output)==XVALUE_CHAR && xrtValueGetChar(output,&scalar) && scalar==0x4f60);
    xrtValueRelease(output); xrtValueRelease(item); item=xrtValueUInt(0x4f60);
    assert(!xl6ArrayErasedSetV1(alias,0,item)); error(XERR_TYPE); assert(xl6ArrayErasedSizeV1(alias)==1);
    xrtValueRelease(item); xrtValueRelease(alias); xrtValueRelease(value);
    value=xl6ArrayCreateV1(OPTIONAL_ARGS); alias=xl6ArrayEraseV1(value,OPTIONAL_ARGS,xl6NullableStringConverterV1()); assert(alias);
    assert(xl6ArrayErasedPushV1(alias,xrtValueNull())); output=xl6ArrayErasedReadV1(alias,0);
    assert(output && xrtValueType(output)==XVALUE_NULL); xrtValueRelease(output);
    item=xrtValueString((xstrview){"",0}); assert(item && xl6ArrayErasedSetV1(alias,0,item));
    output=xl6ArrayErasedReadV1(alias,0); text(output,"",0); xrtValueRelease(output);
    xrtValueRelease(item); xrtValueRelease(alias); xrtValueRelease(value);
    value=xl6ArrayCreateV1(REF_ARGS); alias=xl6ArrayEraseV1(value,REF_ARGS,NULL); item=xrtValueArray(); assert(alias && item);
    assert(xl6ArrayErasedPushV1(alias,item)); output=xl6ArrayErasedReadV1(alias,0); assert(output==item); xrtValueRelease(output);
    assert(xrtValueArrayAppendNew(item,xrtValueInt(42)));
    output=xl6ArrayErasedTakeV1(alias,0); assert(output==item && xrtValueCount(output)==1); xrtValueRelease(output);
    assert(!xl6ArrayEraseV1(value,REF_ARGS,xl6StringConverterV1())); error(XERR_ARGUMENT);
    xrtValueRelease(item); xrtValueRelease(alias); xrtValueRelease(value);
}
static void profileVectors(void)
{
    const xrttype* types[]={xl6NullableBoolTypeV1(),xl6NullableI64TypeV1(),xl6NullableU64TypeV1(),
        xl6NullableF32TypeV1(),xl6NullableF64TypeV1(),xl6NullableTimeTypeV1(),xl6NullableCharTypeV1()};
    const xvalueconverter* converters[]={xl6NullableBoolConverterV1(),xl6NullableI64ConverterV1(),xl6NullableU64ConverterV1(),
        xl6NullableF32ConverterV1(),xl6NullableF64ConverterV1(),xl6NullableTimeConverterV1(),xl6NullableCharConverterV1()};
    xvalue* inputs[]={xrtValueBool(true),xrtValueInt(INT64_MIN),xrtValueUInt(UINT64_MAX),
        xrtValueFloat(-0.0),xrtValueFloat(INFINITY),xrtValueTime(INT64_MAX),xrtValueChar(0x10ffff)};
    for(size_t i=0;i<7;++i) {
        const xrttype* type=types[i];
        xvalue* value=xl6ArrayCreateV1(1010+i,2010+i,1,type->Size,type);
        xvalue* alias=xl6ArrayEraseV1(value,1010+i,2010+i,1,type->Size,type,converters[i]); assert(alias && inputs[i]);
        assert(xl6ArrayErasedPushV1(alias,xrtValueNull()) && xl6ArrayErasedPushV1(alias,inputs[i]));
        xvalue* absent=xl6ArrayErasedReadV1(value,0),*output=xl6ArrayErasedTakeV1(alias,-1);
        assert(absent && xrtValueType(absent)==XVALUE_NULL && output && xrtValueType(output)==xrtValueType(inputs[i]) && xrtValueEqual(output,inputs[i]));
        if(i==3) { double number; assert(xrtValueGetFloat(output,&number) && signbit(number)); }
        assert(xl6ArrayErasedSizeV1(value)==1);
        xrtValueRelease(absent); xrtValueRelease(output); xrtValueRelease(inputs[i]); xrtValueRelease(alias); xrtValueRelease(value);
    }
}
static void foreignDrop(void* raw, void* user) { (void)raw; (void)user; }
static const xvaluehandleops foreignOps={.Drop=foreignDrop};
static void rejectionVectors(void)
{
    XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout={0}; const xvalueconverter* converter=NULL;
    xvalue* value=xl6ArrayCreateV1(I64_ARGS); assert(value);
    assert(xl6SequenceBeginErasedArrayV1(value,&pin,&layout,&converter)==-1); error(XERR_STATE);
    xvalue* alias=xl6ArrayEraseV1(value,I64_ARGS,NULL); assert(alias);
    pin=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,I64_ARGS); assert(pin);
    assert(!xl6ArrayErasedReadV1(value,0)); error(XERR_STATE); xl6SequenceEndV1(pin);
    static const xvalueconverter alternate={0};
    assert(!xl6ArrayEraseV1(value,I64_ARGS,&alternate)); error(XERR_ARGUMENT);
    xvalue* stack=xl6StackCreateV1(I64_ARGS); assert(stack);
    assert(!xl6ArraySizeV1(stack,I64_ARGS,false)); error(XERR_ARGUMENT);
    assert(!xl6StackSizeV1(value,I64_ARGS,false)); error(XERR_ARGUMENT);
    assert(!xl6ArrayEqualsV1(stack,stack,I64_ARGS)); error(XERR_ARGUMENT);
    assert(!xl6StackEqualsV1(value,value,I64_ARGS)); error(XERR_ARGUMENT);
    assert(!xl6ArrayAppendV1(value,stack,I64_ARGS)); error(XERR_ARGUMENT);
    assert(!xl6ArrayIteratorCreateV1(stack,I64_ARGS,3001)); error(XERR_ARGUMENT);
    xvalue* cursor=xl6ArrayIteratorCreateV1(value,I64_ARGS,3001); assert(cursor);
    xrtSetErrorKind(XERR_IO); const xerror* ambient=xrtGetError();
    assert(xl6SequenceBeginErasedArrayV1(stack,&pin,&layout,&converter)==0 && xrtGetError()==ambient);
    assert(xl6SequenceBeginErasedArrayV1(cursor,&pin,&layout,&converter)==0 && xrtGetError()==ambient);
    assert(xl6SequenceBeginErasedArrayV1(xrtValueNull(),&pin,&layout,&converter)==0 && xrtGetError()==ambient);
    void* foreignRaw=(void*)(uintptr_t)1;
    xvalue* foreign=xrtValueHandleTake(&foreignRaw,&foreignOps,NULL); assert(foreign);
    assert(xrtValueTypeIdBind(foreign,1001));
    assert(xl6SequenceBeginErasedArrayV1(foreign,&pin,&layout,&converter)==0 && xrtGetError()==ambient);
    xrtClearError();
    assert(!xl6ArrayErasedReadV1(stack,0)); error(XERR_TYPE);
    assert(!xl6ArrayErasedReadV1(cursor,0)); error(XERR_TYPE);
    assert(!xl6ArrayErasedReadV1(xrtValueNull(),0)); error(XERR_TYPE);
    assert(!xl6ArrayErasedReadV1(foreign,0)); error(XERR_TYPE); xrtValueRelease(foreign);
    xvalueconverter contextual=*xl6StringConverterV1(); contextual.Context=value;
    xvalue* other=xl6ArrayCreateV1(TEXT_ARGS); assert(other);
    assert(!xl6ArrayEraseV1(other,TEXT_ARGS,&contextual)); error(XERR_ARGUMENT);
    xrtValueRelease(other); xrtValueRelease(cursor); xrtValueRelease(stack); xrtValueRelease(alias); xrtValueRelease(value);
}
static xvalue* callbackOwner;
static xvalue* releasingFrom(const xrttype* type, const void* source, ptr context)
{
    (void)context;
    assert(!xl6ArrayErasedReadV1(callbackOwner,0)); error(XERR_STATE);
    xvalue* owner=callbackOwner; callbackOwner=NULL; xrtValueRelease(owner);
    return xl6StringConverterV1()->FromTyped(type,source,NULL);
}
static xvalue* failingFrom(const xrttype* type, const void* source, ptr context)
{ (void)type; (void)source; (void)context; xrtSetErrorKind(XERR_IO); return NULL; }
static bool textTo(const xvalue* source, const xrttype* type, ptr target, ptr context)
{ (void)context; return xl6StringConverterV1()->ToTyped(source,type,target,NULL); }
static const xvalueconverter releasing={.ToTyped=textTo,.FromTyped=releasingFrom};
static const xvalueconverter failing={.ToTyped=textTo,.FromTyped=failingFrom};
static bool releasingTo(const xvalue* source, const xrttype* type, ptr target, ptr context)
{
    (void)context;
    assert(!xl6ArrayErasedPushV1(callbackOwner,source)); error(XERR_STATE);
    xvalue* owner=callbackOwner; callbackOwner=NULL; xrtValueRelease(owner);
    return xl6StringConverterV1()->ToTyped(source,type,target,NULL);
}
static xvalue* textFrom(const xrttype* type, const void* source, ptr context)
{ (void)context; return xl6StringConverterV1()->FromTyped(type,source,NULL); }
static const xvalueconverter releasingWrite={.ToTyped=releasingTo,.FromTyped=textFrom};
static void callbackVectors(void)
{
    xstrview input={"a\0b",3};
    for(unsigned mode=0;mode<2;++mode) {
        xvalue* value=xl6ArrayCreateV1(TEXT_ARGS); assert(value && xl6ArrayPushV1(value,TEXT_ARGS,&input));
        xvalue* alias=xl6ArrayEraseV1(value,TEXT_ARGS,&releasing); assert(alias);
        xvalue* weak=xrtValueWeakRef(value); assert(weak); xrtValueRelease(value); callbackOwner=alias;
        xvalue* output=mode ? xl6ArrayErasedTakeV1(alias,0) : xl6ArrayErasedReadV1(alias,0);
        text(output,"a\0b",3); assert(!callbackOwner && xrtValueWeakRefExpired(weak));
        xrtValueRelease(output); xrtValueRelease(weak);
    }
    xvalue* value=xl6ArrayCreateV1(TEXT_ARGS); assert(value && xl6ArrayPushV1(value,TEXT_ARGS,&input));
    xvalue* alias=xl6ArrayEraseV1(value,TEXT_ARGS,&failing); assert(alias);
    assert(!xl6ArrayEraseV1(value,TEXT_ARGS,xl6StringConverterV1())); error(XERR_ARGUMENT);
    assert(!xl6ArrayErasedReadV1(alias,0)); error(XERR_IO);
    assert(!xl6ArrayErasedTakeV1(alias,0)); error(XERR_IO); assert(xl6ArrayErasedSizeV1(alias)==1);
    xrtValueRelease(alias); xrtValueRelease(value);
    for(unsigned mode=0;mode<3;++mode) {
        value=xl6ArrayCreateV1(TEXT_ARGS); assert(value && xl6ArrayPushV1(value,TEXT_ARGS,&input));
        alias=xl6ArrayEraseV1(value,TEXT_ARGS,&releasingWrite); xvalue* weak=xrtValueWeakRef(value);
        xvalue* item=xrtValueString(input); assert(alias && weak && item); xrtValueRelease(value); callbackOwner=alias;
        assert(mode==0 ? xl6ArrayErasedSetV1(alias,0,item) : mode==1 ? xl6ArrayErasedPushV1(alias,item) : xl6ArrayErasedInsertV1(alias,0,item));
        assert(!callbackOwner && xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); xrtValueRelease(item);
    }
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref, void* context)
{ (void)context; const xrtownershipadapterv1* adapter=xl6SequenceOwnershipAdapterV1(ref); return adapter ? adapter : xrtValueOwnershipAdapterV1(ref); }
static void graph(void)
{
    xvalue* value=xl6ArrayCreateV1(REF_ARGS),*alias=xl6ArrayEraseV1(value,REF_ARGS,NULL); assert(alias);
    xvalue* weak=xrtValueWeakRef(value); assert(weak && xl6ArrayErasedPushV1(alias,value) && xl6ArrayErasedPushV1(value,alias));
    xrtValueRelease(alias); xrtValueRelease(value); XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
}
static void fault(const char* operation, size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* value=xl6ArrayCreateV1(TEXT_ARGS),*alias=xl6ArrayEraseV1(value,TEXT_ARGS,xl6StringConverterV1());
    xvalue* replacement=xrtValueString((xstrview){"new\0text",8}),*result=NULL;
    xstrview old={"old\0text",8}; assert(value && alias && replacement && xl6ArrayPushV1(value,TEXT_ARGS,&old));
    assert(xrtMemDebugFailAfter(point)); bool ok=false;
    if(!strcmp(operation,"erased-read")) { result=xl6ArrayErasedReadV1(alias,0); ok=result!=NULL; }
    else if(!strcmp(operation,"erased-take")) { result=xl6ArrayErasedTakeV1(alias,0); ok=result!=NULL; }
    else if(!strcmp(operation,"erased-clone")) { result=xl6ArrayErasedCloneV1(alias); ok=result!=NULL; }
    else if(!strcmp(operation,"erased-set")) ok=xl6ArrayErasedSetV1(alias,0,replacement);
    else if(!strcmp(operation,"erased-push")) ok=xl6ArrayErasedPushV1(alias,replacement);
    else if(!strcmp(operation,"erased-insert")) ok=xl6ArrayErasedInsertV1(alias,0,replacement);
    else abort();
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    assert(hit ? !ok && xrtErrorKind(xrtGetError())==XERR_MEMORY : ok && !xrtGetError());
    xerror* saved=xrtTakeError();
    int64_t expected=hit ? 1 : !strcmp(operation,"erased-take") ? 0 :
        (!strcmp(operation,"erased-push") || !strcmp(operation,"erased-insert")) ? 2 : 1;
    assert(xl6ArrayErasedSizeV1(value)==expected);
    if(expected) { xvalue* check=xl6ArrayErasedReadV1(alias,0); text(check,!hit && (!strcmp(operation,"erased-set") || !strcmp(operation,"erased-insert")) ? "new\0text" : "old\0text",8); xrtValueRelease(check); }
    xrtSetErrorTake(saved); const xerror* ambient=xrtGetError();
    xrtValueRelease(result); xrtValueRelease(replacement); xrtValueRelease(alias); xrtValueRelease(value);
    assert(xrtGetError()==ambient); xrtClearError(); balanced(&before);
    printf("array-erased %s point=%zu hit=%d balanced\n",operation,point,hit);
}
int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true));
    if(argc==3) { fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    indexVectors(); aliasVectors(); precisionVectors(); profileVectors(); rejectionVectors(); callbackVectors(); graph(); balanced(&before);
    puts("array-erased native vectors balanced"); return 0;
}
