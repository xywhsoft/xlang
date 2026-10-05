#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_stack_batch.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_nullable_string.h"
#include "src/runtime/xl6_scalar_type_abi.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union Slot { int64_t integer; uint16_t c; xstrview text; xvalue* value; XL6_NullableI64V1 nullable; } Slot;
typedef struct Spec { unsigned profile,transfer; const xrttype* type; uint64_t element,stack,array; } Spec;
#define PHYSICAL(s) (s)->element,(s)->transfer,(s)->type->Size,(s)->type
#define STACK(s) (s)->stack,PHYSICAL(s)
#define ARRAY(s) (s)->array,PHYSICAL(s)
static unsigned Checks;
static Spec spec(unsigned profile)
{
    const xrttype* types[]={xrtTypeInt64(),xl6StringTypeV1(),xl6NullableStringTypeV1(),
        xrtTypeValue(),xrtTypeValue(),xrtTypeValue(),xl6NullableI64TypeV1(),xrtTypeUInt16()};
    assert(profile<8);
    return (Spec){profile,profile==1 || profile==2 ? 2u : profile>=3 && profile<=5 ? 3u : 1u,
        types[profile],UINT64_C(100)+profile,UINT64_C(200)+profile,UINT64_C(300)+profile};
}
static xvalue* create(const Spec* s)
{
    if (s->profile==7) return xl6ArrayCScalarCreateV1(s->array,s->element,s->type,s->element);
    if (s->profile==5) {
        const uint64_t identities[]={777};
        XL6_NominalAdmissionV1 schema={.kind=XL6_NOMINAL_ADMIT_EXACT_ID,.identities=identities,.count=1};
        return xl6ArrayNominalCreateV2(s->array,s->element,&schema,true,777);
    }
    if (s->transfer==3) return xl6ArrayReferenceCreateV2(s->array,s->element,
        s->profile==4 ? XL6_VALUE_ADMIT_BYTES : XL6_VALUE_ADMIT_ANY,false,s->profile==4 ? s->element : 0);
    unsigned rule=s->profile==1 || s->profile==2 ? XL6_VALUE_ADMIT_STRING : XL6_VALUE_ADMIT_I8;
    return xl6ArrayPrimitiveCreateV2(s->array,s->element,s->transfer,s->type->Size,s->type,rule,
        s->profile==2 || s->profile==6,s->profile==2 || s->profile==6 ? s->element+1000 : s->element);
}
static Slot item(const Spec* s,int64_t number)
{
    Slot result; memset(&result,0,sizeof result);
    if (s->profile==7) result.c=(uint16_t)number;
    else if (s->profile==6) result.nullable=(XL6_NullableI64V1){true,number};
    else if (s->transfer==1) result.integer=number;
    else if (s->transfer==2) result.text=(xstrview){"a\0你",5};
    else {
        result.value=s->profile==4 ? xrtValueBytes((xbytesview){(const uint8_t*)"a\0",2}) :
            s->profile==5 ? xrtValueObject() : xrtValueInt(number);
        assert(result.value);
        if (s->profile==5) assert(xrtValueTypeIdBind(result.value,777));
        if (s->profile==4) assert(xrtValueTypeIdBind(result.value,s->element));
    }
    return result;
}
static void drop(const Spec* s,Slot* value)
{
    if (s->transfer==3) xrtValueRelease(value->value);
    else if (s->transfer==2 && value->text.Data) xrtTypeDropValue(s->type,value);
    memset(value,0,sizeof *value);
}
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); ++Checks; }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static void fill(const Spec* s,xvalue* stack,xvalue* array)
{
    for (int64_t i=1;i<=3;++i) {
        Slot value=item(s,i);
        assert(xl6ArrayPushV1(array,ARRAY(s),&value));
        if (stack) assert(xl6StackPushV1(stack,STACK(s),&value));
        /* Literal text is a borrowed input, not an owned slot. */
        if (s->transfer==3) xrtValueRelease(value.value);
    }
}
static xarray storage(const Spec* s,xvalue* value,bool stack)
{
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,stack ? XL6_SEQUENCE_STACK_V1 : XL6_SEQUENCE_ARRAY_V1,
        stack ? s->stack : s->array,PHYSICAL(s)); assert(pin);
    xarray result=xl6SequenceStorageV1(pin)->Storage; xl6SequenceEndV1(pin); return result;
}
static void unchanged(const Spec* s,xvalue* value,bool stack,xarray before)
{
    xarray after=storage(s,value,stack);
    assert(after.Count==before.Count && after.Capacity==before.Capacity &&
        after.Data==before.Data && after.Allocation==before.Allocation); ++Checks;
}
static void values(const Spec* s,xvalue* array,int64_t start,int64_t count)
{
    assert(xl6ArraySizeV1(array,ARRAY(s),false)==count);
    for (int64_t i=0;i<count;++i) {
        Slot value; memset(&value,0,sizeof value);
        assert(xl6ArrayReadV1(array,ARRAY(s),i,&value));
        if (s->profile==7) assert(value.c==start-i);
        else if (s->profile==6) assert(value.nullable.hasValue && value.nullable.value==start-i);
        else if (s->transfer==1) assert(value.integer==start-i);
        else if (s->transfer==2) assert(value.text.Size==5 && !memcmp(value.text.Data,"a\0你",5));
        else if (s->profile==3) { int64_t number; assert(xrtValueGetInt(value.value,&number) && number==start-i); }
        else if (s->profile==4) { xbytesview data; assert(xrtValueGetBytes(value.value,&data) && data.Size==2 &&
            !memcmp(data.Data,"a\0",2) && xrtValueTypeId(value.value)==s->element); }
        else assert(xrtValueTypeId(value.value)==777);
        drop(s,&value); ++Checks;
    }
}
static void vectors(const Spec* s)
{
    xvalue* stack=xl6StackCreateV1(STACK(s)),*source=create(s),*target=create(s); assert(stack && source && target);
    fill(s,NULL,source);
    assert(xl6StackPushBatchV1(stack,STACK(s),source,s->array));
    assert(xl6StackSizeV1(stack,STACK(s),false)==3 && xl6ArraySizeV1(source,ARRAY(s),false)==3);
    xarray saved=storage(s,stack,true);
    assert(xl6StackExtractBatchV1(stack,STACK(s),target,s->array,1,INT64_MAX,false));
    values(s,target,2,2); unchanged(s,stack,true,saved);
    assert(xl6ArrayClearV1(target,ARRAY(s)));
    xrtSetErrorKind(XERR_STATE); const xerror* pending=xrtGetError(); assert(pending);
    assert(xl6StackExtractBatchV1(stack,STACK(s),target,s->array,0,INT64_MAX,true));
    assert(xrtGetError()==pending); xrtClearError(); values(s,target,3,3);
    assert(xl6StackSizeV1(stack,STACK(s),false)==0);
    assert(!xl6StackExtractBatchV1(stack,STACK(s),target,s->array,0,1,true)); error(XERR_ARGUMENT);
    assert(xl6ArrayClearV1(target,ARRAY(s)));
    assert(xl6StackExtractBatchV1(stack,STACK(s),target,s->array,0,0,true)); values(s,target,0,0);
    assert(!xl6StackExtractBatchV1(stack,STACK(s),target,s->array,-1,1,false)); error(XERR_RANGE);
    assert(!xl6StackExtractBatchV1(stack,STACK(s),target,s->array,0,-1,false)); error(XERR_RANGE);
    assert(!xl6StackExtractBatchV1(stack,STACK(s),target,s->array,1,1,true)); error(XERR_ARGUMENT);
    assert(!xl6StackExtractBatchV1(stack,STACK(s),target,s->array,1,1,false)); error(XERR_RANGE);
    assert(!xl6StackPushBatchV1(source,STACK(s),source,s->array)); error(XERR_ARGUMENT);
    assert(!xl6StackPushBatchV1(stack,STACK(s),stack,s->array)); error(XERR_ARGUMENT);
    assert(!xl6StackPushBatchV1(stack,STACK(s),source,s->array+1)); error(XERR_ARGUMENT);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_ARRAY_V1,ARRAY(s)); assert(pin);
    assert(!xl6StackPushBatchV1(stack,STACK(s),source,s->array)); error(XERR_STATE); xl6SequenceEndV1(pin);
    pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,STACK(s)); assert(pin);
    assert(!xl6StackExtractBatchV1(stack,STACK(s),target,s->array,0,1,true)); error(XERR_STATE); xl6SequenceEndV1(pin);
    xrtValueRelease(stack); xrtValueRelease(source);
    /* Published metadata and native values survive their source owner. */
    assert(xl6ArrayResizeV1(target,ARRAY(s),2)); xrtValueRelease(target);
}
static void domain(void)
{
    Spec s=spec(0); xvalue* stack=xl6StackCreateV1(STACK(&s)),*target=create(&s); assert(stack && target);
    int64_t good=1,bad=128; assert(xl6StackPushV1(stack,STACK(&s),&good) && xl6StackPushV1(stack,STACK(&s),&bad));
    xarray a=storage(&s,stack,true),b=storage(&s,target,false);
    assert(!xl6StackExtractBatchV1(stack,STACK(&s),target,s.array,0,2,true)); error(XERR_TYPE);
    unchanged(&s,stack,true,a); unchanged(&s,target,false,b);
    assert(!xl6StackExtractBatchV1(stack,STACK(&s),target,s.array,0,2,false)); error(XERR_TYPE);
    unchanged(&s,stack,true,a); unchanged(&s,target,false,b);
    xrtValueRelease(stack); xrtValueRelease(target);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    (void)context; const xrtownershipadapterv1* adapter=xl6SequenceOwnershipAdapterV1(ref);
    return adapter ? adapter : xrtValueOwnershipAdapterV1(ref);
}
static void retire(xvalue* value)
{
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); ++Checks;
}
static void graphs(void)
{
    Spec s=spec(3);
    for (unsigned i=0;i<3;++i) {
        xvalue* stack=xl6StackCreateV1(STACK(&s)),*array=create(&s); assert(stack && array);
        xvalue* cycle=i==2 ? stack : array;
        xvalue* weak=xrtValueWeakRef(cycle); assert(weak);
        if (i==2) {
            assert(xl6ArrayPushV1(array,ARRAY(&s),&stack));
            assert(xl6StackPushBatchV1(stack,STACK(&s),array,s.array));
        } else {
            assert(xl6StackPushV1(stack,STACK(&s),&array));
            assert(xl6StackExtractBatchV1(stack,STACK(&s),array,s.array,0,1,i==0));
        }
        xrtValueRelease(array); xrtValueRelease(stack);
        retire(cycle); assert(xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); ++Checks;
    }
}
static void fault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* stack=xl6StackCreateV1(STACK(s)),*source=create(s),*target=NULL; assert(stack && source);
    fill(s,stack,source);
    bool prepare=!strcmp(operation,"prepare-pop");
    if (!prepare) { target=create(s); assert(target); }
    xarray a=storage(s,stack,true),b=target ? storage(s,target,false) : (xarray){0};
    assert(xrtMemDebugFailAfter(point)); bool ok=false;
    if (prepare) target=create(s);
    if (!strcmp(operation,"push")) ok=xl6StackPushBatchV1(stack,STACK(s),source,s->array);
    else if (target) ok=xl6StackExtractBatchV1(stack,STACK(s),target,s->array,0,3,
        !strcmp(operation,"pop") || prepare);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) {
        assert(!ok); error(XERR_MEMORY);
        if (strcmp(operation,"push")) unchanged(s,stack,true,a);
        else assert(xl6StackSizeV1(stack,STACK(s),false)==3);
        if (target && !prepare) unchanged(s,target,false,b);
        if (target && prepare) assert(xl6ArraySizeV1(target,ARRAY(s),false)==0);
    } else {
        assert(ok && !xrtGetError());
        if (strcmp(operation,"push")) values(s,target,3,3);
        assert(xl6StackSizeV1(stack,STACK(s),false)==(!strcmp(operation,"push") ? 6 : !strcmp(operation,"peek") ? 3 : 0));
    }
    xrtValueRelease(target); xrtValueRelease(stack); xrtValueRelease(source);
    balanced(&before); printf("profile=%u operation=%s point=%zu hit=%d balanced\n",s->profile,operation,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==4) { Spec s=spec((unsigned)strtoul(argv[1],NULL,10)); fault(&s,argv[2],(size_t)strtoull(argv[3],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (unsigned i=0;i<8;++i) { Spec s=spec(i); vectors(&s); }
    domain(); graphs(); balanced(&before); printf("native Stack batch: 8 profiles, %u vectors balanced\n",Checks); return 0;
}
