#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_nullable_string.h"
#ifdef XL6_TEST_MIXED_STACK
#include "src/runtime/xl6_stack.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const xrttype* faultTextType;
#define TEXT_ARGS UINT64_C(401),UINT64_C(501),2,sizeof(xstrview),(faultTextType ? faultTextType : xrtTypeStringView())
#define I64_ARGS UINT64_C(402),UINT64_C(502),1,sizeof(int64_t),xrtTypeInt64()
#define REF_ARGS UINT64_C(403),UINT64_C(503),3,sizeof(xvalue*),xrtTypeValue()
#define BYTES_ARGS UINT64_C(404),UINT64_C(504),3,sizeof(xvalue*),xrtTypeValue()
#define CHAR_ARGS UINT64_C(405),UINT64_C(505),1,sizeof(uint32_t),xrtTypeUInt32()
#define U32_ARGS UINT64_C(406),UINT64_C(506),1,sizeof(uint32_t),xrtTypeUInt32()
#define STACK_ARGS UINT64_C(407),UINT64_C(503),3,sizeof(xvalue*),xrtTypeValue()
#define NULLABLE_TEXT_ARGS UINT64_C(408),UINT64_C(508),2,sizeof(xstrview),xl6NullableStringTypeV1()

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount &&
        before->DoubleFreeCount == after.DoubleFreeCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount);
    assert(!xrtGetError());
}
static void error(xerrkind kind)
{ assert(xrtErrorKind(xrtGetError()) == kind); xrtClearError(); }
static void exact(xstrview value, const char* data, size_t size)
{ assert(value.Size == size && (!size || !memcmp(value.Data,data,size))); }
static void numbers(const xvalue* value, const int64_t* expected, size_t count)
{
    assert(xl6ArraySizeV1(value,I64_ARGS,false) == (int64_t)count);
    for (size_t i=0;i<count;++i) {
        int64_t output=-1; xl6ArrayReadV1(value,I64_ARGS,(int64_t)i,&output);
        assert(!xrtGetError() && output == expected[i]);
    }
}
static void scalarVectors(void)
{
    xvalue* value=xl6ArrayCreateV1(I64_ARGS); assert(value);
    for (int64_t i=0;i<8;++i) assert(xl6ArrayPushV1(value,I64_ARGS,&i));
    int64_t item=77; assert(xl6ArraySetV1(value,I64_ARGS,2,&item));
    item=91; assert(xl6ArrayInsertV1(value,I64_ARGS,1,&item));
    const int64_t inserted[]={0,91,1,77,3,4,5,6,7}; numbers(value,inserted,9);
    int64_t output=-1; xl6ArrayTakeV1(value,I64_ARGS,1,&output); assert(output==91 && !xrtGetError());
    assert(xl6ArrayRemoveV1(value,I64_ARGS,2,2));
    assert(xl6ArrayReverseV1(value,I64_ARGS) && xl6ArraySwapV1(value,I64_ARGS,0,5));
    const int64_t changed[]={0,6,5,4,1,7}; numbers(value,changed,6);
    item=4; assert(xl6ArrayFindV1(value,I64_ARGS,&item)==3);
    item=100; assert(xl6ArrayFindV1(value,I64_ARGS,&item)==-1 && !xrtGetError());
    assert(xl6ArrayAppendV1(value,value,I64_ARGS));
    const int64_t doubled[]={0,6,5,4,1,7,0,6,5,4,1,7}; numbers(value,doubled,12);
    xvalue* copy=xl6ArrayCloneV1(value,I64_ARGS); assert(copy && xl6ArrayEqualsV1(value,copy,I64_ARGS));
    item=9; assert(xl6ArraySetV1(copy,I64_ARGS,0,&item) && !xl6ArrayEqualsV1(value,copy,I64_ARGS));
    numbers(value,doubled,12); xrtValueRelease(copy);
    assert(xl6ArrayResizeV1(value,I64_ARGS,14)); output=-1;
    xl6ArrayReadV1(value,I64_ARGS,13,&output); assert(output==0 && !xrtGetError());
    assert(xl6ArrayResizeV1(value,I64_ARGS,3) && xl6ArrayReserveV1(value,I64_ARGS,4096));
    assert(xl6ArraySizeV1(value,I64_ARGS,true)>=4096);
    assert(xl6ArrayTrimV1(value,I64_ARGS) && xl6ArraySizeV1(value,I64_ARGS,true)==3);
    int64_t capacity=xl6ArraySizeV1(value,I64_ARGS,true);
    xl6ArrayClearV1(value,I64_ARGS); assert(xl6ArraySizeV1(value,I64_ARGS,false)==0);
    assert(xl6ArraySizeV1(value,I64_ARGS,true)==capacity);
    assert(xl6ArrayRemoveV1(value,I64_ARGS,0,0) && xl6ArrayReverseV1(value,I64_ARGS));
    assert(xl6ArrayTrimV1(value,I64_ARGS) && xl6ArraySizeV1(value,I64_ARGS,true)==0);
    output=73; xl6ArrayReadV1(value,I64_ARGS,0,&output); assert(output==73); error(XERR_RANGE);
    xl6ArrayTakeV1(value,I64_ARGS,INT64_MAX,&output); assert(output==73); error(XERR_RANGE);
    assert(!xl6ArraySetV1(value,I64_ARGS,-1,&item)); error(XERR_RANGE);
    assert(!xl6ArrayInsertV1(value,I64_ARGS,1,&item)); error(XERR_RANGE);
    assert(!xl6ArrayRemoveV1(value,I64_ARGS,0,1)); error(XERR_RANGE);
    assert(!xl6ArrayRemoveV1(value,I64_ARGS,INT64_MAX,INT64_MAX)); error(XERR_RANGE);
    assert(!xl6ArraySwapV1(value,I64_ARGS,0,0)); error(XERR_RANGE);
    assert(!xl6ArrayResizeV1(value,I64_ARGS,-1)); error(XERR_RANGE);
    assert(!xl6ArrayReserveV1(value,I64_ARGS,-1)); error(XERR_RANGE);
    xrtValueRelease(value);

    const uint32_t scalars[]={0,65,0x4F60,0x10FFFF};
    value=xl6ArrayCreateV1(CHAR_ARGS); assert(value);
    for (size_t i=0;i<4;++i) assert(xl6ArrayPushV1(value,CHAR_ARGS,&scalars[i]));
    uint32_t observed=19;
    xl6ArrayReadV1(value,U32_ARGS,0,&observed); assert(observed==19); error(XERR_ARGUMENT);
    xl6ArrayReadV1(value,CHAR_ARGS,3,&observed); assert(observed==0x10FFFF && !xrtGetError());
    assert(!xl6ArrayPushV1(value,CHAR_ARGS,NULL)); error(XERR_ARGUMENT);
    xrtValueRelease(value);
    assert(!xl6ArrayCreateV1(0,502,1,sizeof(int64_t),xrtTypeInt64())); error(XERR_ARGUMENT);
    assert(!xl6ArrayCreateV1(402,0,1,sizeof(int64_t),xrtTypeInt64())); error(XERR_ARGUMENT);
    assert(!xl6ArrayCreateV1(402,502,1,1,xrtTypeInt64())); error(XERR_ARGUMENT);
    assert(!xl6ArrayCreateV1(402,502,4,sizeof(int64_t),xrtTypeInt64())); error(XERR_ARGUMENT);
    assert(!xl6ArrayCreateV1(402,502,1,sizeof(xvalue*),xrtTypeValue())); error(XERR_ARGUMENT);
    assert(!xl6ArraySizeV1(NULL,I64_ARGS,false)); error(XERR_ARGUMENT);
    value=xrtValueNull(); assert(value); assert(!xl6ArraySizeV1(value,I64_ARGS,false)); error(XERR_ARGUMENT);
    xrtValueRelease(value);
    /* The shared owner must reject, and consume, healthy storage whose actual
     * physical descriptor disagrees with the closed precise layout tuple. */
    xtypedarray* raw=xrtTypedArrayCreate(xrtTypeUInt32()); assert(raw);
    assert(!xl6SequenceWrapV1(raw,I64_ARGS)); error(XERR_ARGUMENT);
    raw=xrtTypedArrayCreate(xrtTypeInt64()); assert(raw);
    assert(!xl6SequenceWrapV1(raw,0,502,1,sizeof(int64_t),xrtTypeInt64())); error(XERR_ARGUMENT);
}
static void textVectors(void)
{
    char data[]={'a',0,'b'}; xstrview input={data,sizeof(data)},output={0};
    xvalue* value=xl6ArrayCreateV1(TEXT_ARGS); assert(value && xl6ArrayPushV1(value,TEXT_ARGS,&input));
    data[0]='z'; xl6ArrayReadV1(value,TEXT_ARGS,0,&output); exact(output,"a\0b",3);
    assert(output.Data!=data && !xrtGetError());
    xvalue* copy=xl6ArrayCloneV1(value,TEXT_ARGS); assert(copy && xl6ArrayEqualsV1(value,copy,TEXT_ARGS));
    assert(xl6ArrayInsertV1(copy,TEXT_ARGS,0,&input) && xl6ArrayFindV1(copy,TEXT_ARGS,&input)==0);
    xl6ArrayTakeV1(copy,TEXT_ARGS,1,&output); exact(output,"a\0b",3); assert(!xrtGetError());
    assert(xl6ArraySizeV1(copy,TEXT_ARGS,false)==1 && !xl6ArrayEqualsV1(value,copy,TEXT_ARGS));
    xrtTypeDropValue(xrtTypeStringView(),&output);
    assert(xl6ArrayAppendV1(value,value,TEXT_ARGS));
    xl6ArrayReadV1(value,TEXT_ARGS,1,&output); exact(output,"a\0b",3);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* saved=xrtGetError();
    xl6ArrayClearV1(value,TEXT_ARGS); assert(xrtGetError()==saved); xrtClearError();
    xrtValueRelease(value); xrtValueRelease(copy); exact(output,"a\0b",3);
    xrtTypeDropValue(xrtTypeStringView(),&output);
}
static void referenceVectors(void)
{
    unsigned char bytes[256]; for (size_t i=0;i<sizeof(bytes);++i) bytes[i]=(unsigned char)i;
    xvalue* item=xrtValueBytes((xbytesview){bytes,sizeof(bytes)});
    xvalue* value=xl6ArrayCreateV1(BYTES_ARGS); assert(item && value && xl6ArrayPushV1(value,BYTES_ARGS,&item));
    xvalue* copy=xl6ArrayCloneV1(value,BYTES_ARGS); assert(copy && xl6ArrayEqualsV1(value,copy,BYTES_ARGS));
    xvalue* output=xrtValueInt(7); assert(output);
    assert(xrtMemDebugFailAfter(0)); xl6ArrayReadV1(value,BYTES_ARGS,0,&output);
    assert(!xrtMemDebugFailTriggered() && output==item && !xrtGetError()); xrtMemDebugFailClear();
    xrtValueRelease(item); xl6ArrayTakeV1(copy,BYTES_ARGS,0,&output); assert(!xrtGetError());
    xrtValueRelease(copy); xrtValueRelease(value);
    xbytesview observed={0}; assert(xrtValueGetBytes(output,&observed));
    assert(observed.Size==sizeof(bytes) && !memcmp(observed.Data,bytes,sizeof(bytes))); xrtValueRelease(output);

    value=xl6ArrayCreateV1(REF_ARGS); item=xrtValueArray(); assert(value && item);
    assert(xl6ArrayPushV1(value,REF_ARGS,&item)); copy=xl6ArrayCloneV1(value,REF_ARGS); assert(copy);
    output=NULL; xl6ArrayReadV1(copy,REF_ARGS,0,&output); assert(output==item && !xrtGetError());
    assert(xrtValueArrayAppendNew(output,xrtValueInt(19)) && xrtValueCount(item)==1);
    assert(xl6ArrayFindV1(value,REF_ARGS,&output)==0 && xl6ArrayEqualsV1(value,copy,REF_ARGS));
    assert(xl6ArrayResizeV1(copy,REF_ARGS,3));
    xvalue* empty=NULL; assert(xl6ArrayFindV1(copy,REF_ARGS,&empty)==1 && !xrtGetError());
    xvalue* nullValue=xrtValueNull(); assert(nullValue);
    assert(xl6ArrayFindV1(copy,REF_ARGS,&nullValue)==1 && !xrtGetError());
    xvalue* filled=xl6ArrayCloneV1(value,REF_ARGS); assert(filled);
    assert(xl6ArrayPushV1(filled,REF_ARGS,&nullValue) && xl6ArrayPushV1(filled,REF_ARGS,&nullValue));
    assert(xl6ArrayEqualsV1(copy,filled,REF_ARGS) && !xrtGetError());
    assert(xl6ArrayFindV1(filled,REF_ARGS,&empty)==1 && !xrtGetError());
    xrtValueRelease(filled); xrtValueRelease(nullValue);
    xrtValueRelease(output); xrtValueRelease(item); xrtValueRelease(value); xrtValueRelease(copy);
}
static void nullableTextVectors(void)
{
    const xrttype* type=xl6NullableStringTypeV1(); assert(xl6NullableStringTypeAbiV1(type));
    assert(xrtTypeId(type->AbiName)==type->Id && xrtTypeValidate(type));
    xstrview absent={0},empty=XRT_STR_INIT(""),text=XRT_STR_INIT("a\0b"),output={0};
    int order=0; uint64 hash1=99,hash2=99;
    assert(xrtTypeCompareValue(type,&absent,&empty,&order) && order<0);
    assert(xrtTypeCompareValue(type,&empty,&absent,&order) && order>0);
    assert(xrtTypeHashValue(type,&absent,&hash1) && xrtTypeHashValue(type,&empty,&hash2) && hash1!=hash2);
    const xvalueconverter* converter=xl6NullableStringConverterV1();
    assert(converter && converter->ToTyped && converter->FromTyped);
    const xstrview vectors[]={absent,empty,text};
    for (size_t i=0;i<3;++i) {
        xvalue* boxed=xrtValueFromTyped(type,&vectors[i],converter); assert(boxed);
        xstrview roundtrip={0};
        assert(xrtValueToTyped(boxed,type,&roundtrip,converter));
        assert(xl6NullableStringEqualV1(vectors[i],roundtrip));
        assert((vectors[i].Data!=NULL)==(roundtrip.Data!=NULL));
        xrtTypeDropValue(type,&roundtrip); xrtValueRelease(boxed);
    }
    xstrview rejected={0};
    assert(!xrtValueToTyped(xrtValueBool(true),type,&rejected,converter)); error(XERR_TYPE);
    assert(!rejected.Data && !rejected.Size);
    xstrview malformed={NULL,1};
    assert(!xrtValueFromTyped(type,&malformed,converter)); error(XERR_ARGUMENT);
    assert(xrtMemDebugFailAfter(0));
    assert(xrtValueFromTyped(type,&absent,converter)==xrtValueNull());
    assert(!xrtMemDebugFailTriggered() && !xrtGetError()); xrtMemDebugFailClear();
    for (size_t point=0;;++point) {
        assert(point<16 && xrtMemDebugFailAfter(point));
        xvalue* boxed=xrtValueFromTyped(type,&text,converter);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (hit) { assert(!boxed); error(XERR_MEMORY); }
        else { assert(boxed); xrtValueRelease(boxed); break; }
    }
    xvalue* boxed=xrtValueString(text); assert(boxed);
    assert(xrtMemDebugFailAfter(0));
    assert(!xrtValueToTyped(boxed,type,&rejected,converter));
    assert(xrtMemDebugFailTriggered() && !rejected.Data && !rejected.Size);
    xrtMemDebugFailClear(); error(XERR_MEMORY); xrtValueRelease(boxed);
    xvalue* value=xl6ArrayCreateV1(NULLABLE_TEXT_ARGS); assert(value);
    assert(xl6ArrayPushV1(value,NULLABLE_TEXT_ARGS,&absent));
    assert(xl6ArrayPushV1(value,NULLABLE_TEXT_ARGS,&empty));
    assert(xl6ArrayPushV1(value,NULLABLE_TEXT_ARGS,&text));
    assert(xl6ArrayFindV1(value,NULLABLE_TEXT_ARGS,&absent)==0 &&
           xl6ArrayFindV1(value,NULLABLE_TEXT_ARGS,&empty)==1);
    xvalue* cloned=xl6ArrayCloneV1(value,NULLABLE_TEXT_ARGS); assert(cloned);
    assert(xl6ArrayEqualsV1(value,cloned,NULLABLE_TEXT_ARGS));
    assert(xl6ArraySetV1(cloned,NULLABLE_TEXT_ARGS,0,&empty));
    assert(!xl6ArrayEqualsV1(value,cloned,NULLABLE_TEXT_ARGS) && !xrtGetError());
    xl6ArrayReadV1(value,NULLABLE_TEXT_ARGS,2,&output); exact(output,"a\0b",3);
    assert(xrtMemDebugFailAfter(0)); xl6ArrayReadV1(value,NULLABLE_TEXT_ARGS,0,&output);
    assert(!xrtMemDebugFailTriggered() && output.Data==NULL && output.Size==0 && !xrtGetError());
    xrtMemDebugFailClear();
    xl6ArrayReadV1(value,NULLABLE_TEXT_ARGS,1,&output); exact(output,"",0);
    const char* prior=output.Data;
    assert(xrtMemDebugFailAfter(0)); xl6ArrayReadV1(value,NULLABLE_TEXT_ARGS,2,&output);
    assert(xrtMemDebugFailTriggered() && output.Data==prior);
    xrtMemDebugFailClear(); error(XERR_MEMORY); exact(output,"",0);
    xstrview invalid={NULL,1};
    assert(!xrtTypeCopyValue(type,&output,&invalid) && output.Data==prior); error(XERR_ARGUMENT);
    xrtTypeDropValue(type,&output);
    assert(xl6ArrayResizeV1(value,NULLABLE_TEXT_ARGS,4));
    xl6ArrayReadV1(value,NULLABLE_TEXT_ARGS,3,&output); assert(!output.Data && !output.Size);
    xrttype forged=*type; forged.Id++;
    assert(!xl6SequenceTupleV1(408,508,2,sizeof(xstrview),&forged)); error(XERR_ARGUMENT);
    forged=*type; forged.AbiName=(xstrview)XRT_STR_INIT("wrong-nullable-string-abi");
    assert(!xl6SequenceTupleV1(408,508,2,sizeof(xstrview),&forged)); error(XERR_ARGUMENT);
    forged=*type; forged.Flags|=XRT_TYPE_FLAG_TRIVIAL_DROP;
    assert(!xl6SequenceTupleV1(408,508,2,sizeof(xstrview),&forged)); error(XERR_ARGUMENT);
    xrtValueRelease(value); xrtValueRelease(cloned);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref, void* context)
{
    (void)context; const xrtownershipadapterv1* adapter=xl6SequenceOwnershipAdapterV1(ref);
    return adapter ? adapter : xrtValueOwnershipAdapterV1(ref);
}
static void retire(xvalue* value)
{
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
}
static void graphVectors(void)
{
    xvalue* value=xl6ArrayCreateV1(REF_ARGS); xvalue* weak=xrtValueWeakRef(value); assert(value && weak);
    assert(xl6ArrayPushV1(value,REF_ARGS,&value) && xl6ArrayPushV1(value,REF_ARGS,&value));
    xrtValueRelease(value);
    xvalue* shell=xrtValueDeepClone(value); assert(shell && shell!=value);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6ArraySizeV1(shell,REF_ARGS,false)==2);
    xrtValueRelease(shell);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
    value=xl6ArrayCreateV1(REF_ARGS); weak=xrtValueWeakRef(value); assert(value && weak);
    assert(xl6ArrayPushV1(value,REF_ARGS,&value)); xrtValueRelease(value);
    retire(value); assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
#ifdef XL6_TEST_MIXED_STACK
    value=xl6ArrayCreateV1(REF_ARGS); xvalue* stack=xl6StackCreateV1(STACK_ARGS);
    weak=xrtValueWeakRef(value); xvalue* stackWeak=xrtValueWeakRef(stack);
    assert(value && stack && weak && stackWeak);
    assert(xl6ArrayPushV1(value,REF_ARGS,&stack) && xl6StackPushV1(stack,STACK_ARGS,&value));
    /* Same physical descriptor, distinct semantic identities: no accidental
     * conversion between Array<T> and Stack<T>. */
    assert(!xl6ArrayAppendV1(value,stack,REF_ARGS)); error(XERR_ARGUMENT);
    xrtValueRelease(stack); xrtValueRelease(value); retire(value);
    assert(xrtValueWeakRefExpired(weak) && xrtValueWeakRefExpired(stackWeak));
    xrtValueRelease(weak); xrtValueRelease(stackWeak);
#endif
}
static xvalue* callbackOwner;
static unsigned callbackDrops,callbackEquals;
static bool cloneHandle(void* source, void** target, void* user)
{ (void)user; *target=source; return true; }
static void dropHandle(void* source, void* user)
{ (void)source; (void)user; ++callbackDrops; }
static uint64_t hashHandle(void* source, void* user)
{ (void)source; (void)user; return 1; }
static bool equalHandle(void* left, void* right, void* user)
{
    (void)left; (void)right; (void)user; ++callbackEquals;
    xl6ArrayClearV1(callbackOwner,REF_ARGS); error(XERR_STATE);
    XL6_OwnershipPlan* plan=NULL;
    /* Value equality is inside a participating mutation: freeze is BUSY,
     * rather than interpreting the temporarily inaccessible graph. */
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(callbackOwner),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_BUSY);
    assert(!plan && !xrtGetError());
    xvalue* value=callbackOwner; callbackOwner=NULL; xrtValueRelease(value); return true;
}
static const xvaluehandleops equalityOps={cloneHandle,dropHandle,hashHandle,equalHandle};
static void reentryVectors(void)
{
    for (unsigned mode=0;mode<2;++mode) {
        xvalue* value=xl6ArrayCreateV1(REF_ARGS),*other=xl6ArrayCreateV1(REF_ARGS); assert(value && other);
        int a=1,b=2; void* pa=&a; void* pb=&b;
        xvalue* left=xrtValueHandleTake(&pa,&equalityOps,NULL);
        xvalue* right=xrtValueHandleTake(&pb,&equalityOps,NULL); assert(left && right);
        assert(xl6ArrayPushV1(value,REF_ARGS,&left) && xl6ArrayPushV1(other,REF_ARGS,&right));
        xrtValueRelease(left); callbackOwner=value;
        if (mode) assert(xl6ArrayFindV1(value,REF_ARGS,&right)==0);
        else assert(xl6ArrayEqualsV1(value,other,REF_ARGS));
        assert(!callbackOwner && !xrtGetError()); xrtValueRelease(right); xrtValueRelease(other);
    }
    assert(callbackEquals==2 && callbackDrops==4);
    xvalue* value=xl6ArrayCreateV1(I64_ARGS); xvalue* shell=xrtValueDeepClone(value); assert(value && shell);
    XL6_Sequence* guard=xl6SequenceBeginV1(value,I64_ARGS); assert(guard);
    assert(!xl6ArrayReserveV1(shell,I64_ARGS,64)); error(XERR_STATE);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR);
    assert(!plan); xrtClearError();
    xl6SequenceEndV1(guard);
    assert(xl6ArrayReserveV1(shell,I64_ARGS,64)); xrtValueRelease(value); xrtValueRelease(shell);
}
static unsigned tailDrops;
static void tailDrop(void* raw, void* user)
{
    (void)raw; (void)user; ++tailDrops;
    xvalue* nested=NULL;
    assert(!xl6ArrayPushV1(callbackOwner,REF_ARGS,&nested)); error(XERR_STATE);
    xvalue* owner=callbackOwner; callbackOwner=NULL; xrtValueRelease(owner);
}
static const xvaluehandleops tailOps={NULL,tailDrop,NULL,NULL};
static void tailVectors(void)
{
    for (unsigned mode=0;mode<4;++mode) {
        xvalue* value=xl6ArrayCreateV1(REF_ARGS); int raw=1; void* pointer=&raw;
        xvalue* item=xrtValueHandleTake(&pointer,&tailOps,NULL); assert(value && item);
        assert(xl6ArrayPushV1(value,REF_ARGS,&item)); xrtValueRelease(item); callbackOwner=value;
        /* All these paths release the caller's last owner from an element
         * destructor. The operation token keeps only the physical wrapper
         * alive, without retaining a dangling xvalue shell after the call. */
        if (mode==0) xl6ArrayClearV1(value,REF_ARGS);
        else if (mode==1) assert(xl6ArrayRemoveV1(value,REF_ARGS,0,1));
        else if (mode==2) { item=NULL; assert(xl6ArraySetV1(value,REF_ARGS,0,&item)); }
        else assert(xl6ArrayResizeV1(value,REF_ARGS,0));
        assert(!callbackOwner && !xrtGetError());
    }
    assert(tailDrops==4);
}
static void graphFault(const char* stage, size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* value=xl6ArrayCreateV1(REF_ARGS),*weak=xrtValueWeakRef(value); assert(value && weak);
    assert(xl6ArrayPushV1(value,REF_ARGS,&value) && xl6ArrayPushV1(value,REF_ARGS,&value));
    xrtValueRelease(value); XL6_OwnershipPlan* plan=NULL;
    bool create=!strcmp(stage,"graph-create"),commit=!strcmp(stage,"graph-commit");
    assert(create || commit || !strcmp(stage,"graph-refresh"));
    if (!create) assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtMemDebugFailAfter(point));
    XL6_OwnershipPlanResult result=create ? xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan) :
        commit ? xl6OwnershipPlanTryCommit(plan) : xl6OwnershipPlanTryRefresh(plan);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    assert(hit ? result==XL6_OWNERSHIP_PLAN_ERROR : result==XL6_OWNERSHIP_PLAN_READY);
    if (hit) {
        error(XERR_MEMORY); assert(xl6ArraySizeV1(value,REF_ARGS,false)==2);
        if (plan) assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        retire(value);
    } else {
        if (!commit) assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanFinish(plan) && xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    }
    assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); balanced(&before);
    printf("array %s point=%zu hit=%d balanced\n",stage,point,hit);
}
static void fault(const char* operation, size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xstrview input={"a\0b",3},replacement={"c\0d",3},output={xrtStrDup("unchanged"),9};
    xvalue* value=xl6ArrayCreateV1(TEXT_ARGS),*source=xl6ArrayCreateV1(TEXT_ARGS),*created=NULL;
    assert(value && source && output.Data && xl6ArrayReserveV1(value,TEXT_ARGS,8));
    assert(xl6ArrayPushV1(value,TEXT_ARGS,&input) && xl6ArrayPushV1(source,TEXT_ARGS,&input));
    assert(xl6ArrayPushV1(source,TEXT_ARGS,&input));
    assert(xrtMemDebugFailAfter(point)); bool ok=false;
    if (!strcmp(operation,"create")) ok=(created=xl6ArrayCreateV1(TEXT_ARGS))!=NULL;
    else if (!strcmp(operation,"reserve")) ok=xl6ArrayReserveV1(value,TEXT_ARGS,32768);
    else if (!strcmp(operation,"push")) ok=xl6ArrayPushV1(value,TEXT_ARGS,&replacement);
    else if (!strcmp(operation,"set")) ok=xl6ArraySetV1(value,TEXT_ARGS,0,&replacement);
    else if (!strcmp(operation,"insert")) ok=xl6ArrayInsertV1(value,TEXT_ARGS,0,&replacement);
    else if (!strcmp(operation,"read")) { xl6ArrayReadV1(value,TEXT_ARGS,0,&output); ok=!xrtGetError(); }
    else if (!strcmp(operation,"take")) { xl6ArrayTakeV1(value,TEXT_ARGS,0,&output); ok=!xrtGetError(); }
    else if (!strcmp(operation,"clone")) ok=(created=xl6ArrayCloneV1(value,TEXT_ARGS))!=NULL;
    else if (!strcmp(operation,"trim")) ok=xl6ArrayTrimV1(value,TEXT_ARGS);
    else if (!strcmp(operation,"resize")) ok=xl6ArrayResizeV1(value,TEXT_ARGS,32768);
    else if (!strcmp(operation,"append")) ok=xl6ArrayAppendV1(value,source,TEXT_ARGS);
    else if (!strcmp(operation,"self-append")) ok=xl6ArrayAppendV1(value,value,TEXT_ARGS);
    else if (!strcmp(operation,"remove")) ok=xl6ArrayRemoveV1(value,TEXT_ARGS,0,1);
    else if (!strcmp(operation,"swap")) ok=xl6ArraySwapV1(value,TEXT_ARGS,0,0);
    else if (!strcmp(operation,"reverse")) ok=xl6ArrayReverseV1(value,TEXT_ARGS);
    else if (!strcmp(operation,"find")) ok=xl6ArrayFindV1(value,TEXT_ARGS,&input)==0 && !xrtGetError();
    else if (!strcmp(operation,"clear")) { xl6ArrayClearV1(value,TEXT_ARGS); ok=!xrtGetError(); }
    else if (!strcmp(operation,"equals")) ok=!xl6ArrayEqualsV1(value,source,TEXT_ARGS) && !xrtGetError();
    else abort();
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit && (ok || xrtErrorKind(xrtGetError())!=XERR_MEMORY))
        fprintf(stderr,"%s point=%zu hit=%d kind=%d ok=%d\n",operation,point,hit,(int)xrtErrorKind(xrtGetError()),ok);
    assert(hit ? !ok && xrtErrorKind(xrtGetError())==XERR_MEMORY : ok && !xrtGetError());
    xerror* failure=xrtTakeError();
    int64_t count=1;
    if (ok) {
        if (!strcmp(operation,"push") || !strcmp(operation,"insert") || !strcmp(operation,"self-append")) count=2;
        else if (!strcmp(operation,"append")) count=3;
        else if (!strcmp(operation,"take") || !strcmp(operation,"remove") || !strcmp(operation,"clear")) count=0;
        else if (!strcmp(operation,"resize")) count=32768;
    }
    assert(xl6ArraySizeV1(value,TEXT_ARGS,false)==count && xl6ArraySizeV1(source,TEXT_ARGS,false)==2);
    if (hit || (strcmp(operation,"read") && strcmp(operation,"take"))) exact(output,"unchanged",9);
    else exact(output,"a\0b",3);
    xstrview check={0};
    if (count) {
        xl6ArrayReadV1(value,TEXT_ARGS,0,&check);
        exact(check,ok && (!strcmp(operation,"set") || !strcmp(operation,"insert")) ? "c\0d" : "a\0b",3);
        xrtTypeDropValue(xrtTypeStringView(),&check);
    }
    xl6ArrayReadV1(source,TEXT_ARGS,0,&check); exact(check,"a\0b",3);
    xrtTypeDropValue(xrtTypeStringView(),&check); xrtTypeDropValue(xrtTypeStringView(),&output);
    xrtSetErrorTake(failure); const xerror* saved=xrtGetError();
    xrtValueRelease(created); xrtValueRelease(value); xrtValueRelease(source);
    assert(xrtGetError()==saved); xrtClearError(); balanced(&before);
    printf("array %s point=%zu hit=%d balanced\n",operation,point,hit);
}
int main(int argc, char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) {
        size_t point=(size_t)strtoull(argv[2],NULL,10);
        if (!strncmp(argv[1],"graph-",6)) graphFault(argv[1],point);
        else {
            const char* operation=argv[1];
            if (!strncmp(operation,"nullable-",9)) { faultTextType=xl6NullableStringTypeV1(); operation+=9; }
            fault(operation,point);
        }
        return 0;
    }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    scalarVectors(); textVectors(); referenceVectors(); nullableTextVectors(); graphVectors(); reentryVectors(); tailVectors(); balanced(&before);
    puts("array native vectors balanced"); return 0;
}
