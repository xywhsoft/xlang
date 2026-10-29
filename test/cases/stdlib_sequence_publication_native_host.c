#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_stack_batch.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_string_type.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Actual resident factories and storage, not a generated-policy substitute.
 * These five profiles exercise publication, native/reference/string transfer,
 * nullable payloads, clone metadata and cross-role ownership. */
typedef union Slot { int64_t integer; xstrview text; xvalue* value; XL6_NullableI64V1 nullable; } Slot;
typedef struct Spec { unsigned profile,transfer; const xrttype* type; uint64_t element; } Spec;
#define TUPLE(s) (s)->element,(s)->transfer,(s)->type->Size,(s)->type
#define ARRAY(s) 1001,TUPLE(s)
#define STACK(s) 2001,TUPLE(s)
static Spec spec(unsigned profile)
{
    const xrttype* types[]={xrtTypeInt64(),xl6StringTypeV1(),xrtTypeValue(),xl6NullableI64TypeV1(),xrtTypeValue()};
    assert(profile<5);
    return (Spec){profile,profile==1 ? 2u : profile==2 || profile==4 ? 3u : 1u,types[profile],3001+profile};
}
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
static xvalue* array(const Spec* s)
{
    if (s->transfer==3) return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,1001,s->element,
        s->profile==2 ? XL6_VALUE_ADMIT_BYTES : XL6_VALUE_ADMIT_ANY,false,s->profile==2 ? s->element : 0);
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,1001,s->element,s->transfer,s->type->Size,s->type,
        s->profile==1 ? XL6_VALUE_ADMIT_STRING : XL6_VALUE_ADMIT_I8,s->profile==3,
        s->profile==3 ? s->element+1000 : s->element);
}
static XL6_SequenceAdmissionV3 policy(const Spec* s,const xvalue* source)
{
    XL6_SequenceAdmissionV3 result={0};
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_ARRAY_V1,ARRAY(s)); assert(owner);
    assert(xl6SequenceAdmissionCopyV3(owner,&result)); xl6SequenceEndV1(owner); assert(result.ops); return result;
}
static bool empty(const XL6_SequenceAdmissionV3* value)
{ return !value->ops && !value->data && !value->elementIdentity && !value->transfer && !value->size && !value->type && !value->boxedIdentity; }
static xvalue* stack(const Spec* s,const xvalue* source)
{
    XL6_SequenceAdmissionV3 owned=policy(s,source);
    xvalue* result=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_STACK_V1,2001,&owned);
    assert(result && empty(&owned)); return result;
}
static Slot item(const Spec* s,bool bad)
{
    Slot value={0};
    if (s->profile==0) value.integer=bad ? 128 : 7;
    else if (s->profile==1) value.text=(xstrview){bad ? NULL : "a\0你",5};
    else if (s->profile==3) value.nullable=(XL6_NullableI64V1){true,bad ? 128 : 7};
    else {
        value.value=s->profile==2 && !bad ? xrtValueBytes((xbytesview){(const uint8_t*)"a\0",2}) : xrtValueInt(7);
        assert(value.value);
    }
    return value;
}
static void dropItem(const Spec* s,Slot* value)
{ if (s->transfer==3) xrtValueRelease(value->value); }
static void checkItem(const Spec* s,const Slot* value)
{
    if (s->profile==0) assert(value->integer==7);
    else if (s->profile==1) assert(value->text.Size==5 && !memcmp(value->text.Data,"a\0你",5));
    else if (s->profile==3) assert(value->nullable.hasValue && value->nullable.value==7);
    else assert(xrtValueType(value->value)==(s->profile==2 ? XVALUE_BYTES : XVALUE_INT));
}
static void vectors(const Spec* s)
{
    xvalue* source=array(s); assert(source); xvalue* owner=stack(s,source);
    Slot good=item(s,false),bad=item(s,true);
    assert(xl6StackPushV1(owner,STACK(s),&good));
    xvalue* clone=xl6StackCloneV1(owner,STACK(s)); assert(clone && xl6StackEqualsV1(owner,clone,STACK(s)));
    /* Both source factory owner and original Stack die. Clone must own its
     * resident domain, not a borrowed schema or unprofiled storage wrapper. */
    xrtValueRelease(owner); xrtValueRelease(source);
    assert(xrtMemDebugFailAfter(0));
    if (s->profile!=4) { assert(!xl6StackPushV1(clone,STACK(s),&bad)); error(XERR_TYPE); }
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6StackSizeV1(clone,STACK(s),false)==1);
    source=array(s); assert(source && xl6ArrayPushV1(source,ARRAY(s),&good));
    assert(xl6StackPushBatchV1(clone,STACK(s),source,1001));
    if (s->profile!=4 && s->profile!=1) {
        /* Correct logical/physical tuple is not evidence of admission. Check
         * all slots before copying: a valid prefix plus invalid last item. */
        xvalue* unprofiled=xl6ArrayCreateV1(ARRAY(s)); assert(unprofiled);
        assert(xl6ArrayPushV1(unprofiled,ARRAY(s),&good));
        assert(xl6ArrayPushV1(unprofiled,ARRAY(s),&bad));
        int64_t capacity=xl6StackSizeV1(clone,STACK(s),true);
        assert(xrtMemDebugFailAfter(0));
        assert(!xl6StackPushBatchV1(clone,STACK(s),unprofiled,1001)); error(XERR_TYPE);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        assert(xl6StackSizeV1(clone,STACK(s),false)==2 && xl6StackSizeV1(clone,STACK(s),true)==capacity);
        xrtValueRelease(unprofiled);
    }
    xvalue* target=array(s); assert(target && xl6StackExtractBatchV1(clone,STACK(s),target,1001,0,2,true));
    assert(xl6StackSizeV1(clone,STACK(s),false)==0 && xl6ArraySizeV1(target,ARRAY(s),false)==2);
    Slot read={0}; assert(xl6ArrayReadV1(target,ARRAY(s),0,&read)); checkItem(s,&read);
    xrtTypeDropValue(xl6SequenceStorageTypeV1(s->transfer,s->type),&read);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(clone,XL6_SEQUENCE_STACK_V1,STACK(s)); assert(pin);
    assert(!xl6StackCloneV1(clone,STACK(s))); error(XERR_STATE); xl6SequenceEndV1(pin);
    assert(!xl6SequenceCloneRoleV1(clone,XL6_SEQUENCE_ARRAY_V1,STACK(s))); error(XERR_ARGUMENT);
    assert(!xl6SequenceCloneRoleV1(clone,XL6_SEQUENCE_CURSOR_V1,STACK(s))); error(XERR_ARGUMENT);
    xrtValueRelease(target); xrtValueRelease(source); xrtValueRelease(clone); dropItem(s,&good); dropItem(s,&bad);
}
static void contracts(const Spec* s)
{
    xvalue* source=array(s); assert(source);
    for (unsigned change=0;change<10;++change) {
        XL6_SequenceAdmissionV3 owned=policy(s,source),saved=owned;
        if (change==0) owned.ops=NULL;
        if (change==1) owned.data=NULL;
        if (change==2) owned.elementIdentity=0;
        if (change==3) owned.transfer=0;
        if (change==4) owned.size=0;
        if (change==5) owned.type=NULL;
        if (change==6) owned.boxedIdentity=0;
        if (change==7) ++owned.size;
        assert(xrtMemDebugFailAfter(0));
        assert(!xl6SequenceCreateAdmittedV1(change==8 ? XL6_SEQUENCE_CURSOR_V1 : XL6_SEQUENCE_STACK_V1,
            change==9 ? 0 : 2001,&owned)); error(XERR_ARGUMENT);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        if (change<7) {
            assert(owned.ops==(change==0 ? NULL : saved.ops) && owned.data==(change==1 ? NULL : saved.data));
            owned=saved; xl6SequenceAdmissionDropV3(&owned);
        } else assert(empty(&owned));
    }
    XL6_SequenceAdmissionV3 owned={0};
    assert(!xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_STACK_V1,2001,&owned)); error(XERR_ARGUMENT); assert(empty(&owned));
    assert(!xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_STACK_V1,2001,NULL)); error(XERR_ARGUMENT);
    xrtValueRelease(source);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{ (void)context; const xrtownershipadapterv1* a=xl6SequenceOwnershipAdapterV1(ref); return a ? a : xrtValueOwnershipAdapterV1(ref); }
static void graph(void)
{
    Spec s=spec(4); xvalue* a=array(&s),*b=stack(&s,a); assert(a && b);
    assert(xl6ArrayPushV1(a,ARRAY(&s),&b) && xl6StackPushV1(b,STACK(&s),&a));
    xvalue* wa=xrtValueWeakRef(a),*wb=xrtValueWeakRef(b); assert(wa && wb);
    xrtValueRelease(a); xrtValueRelease(b); XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(b),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(wa) && xrtValueWeakRefExpired(wb)); xrtValueRelease(wa); xrtValueRelease(wb);
}
static void fault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* source=array(s); assert(source); Slot good=item(s,false);
    xvalue* owner=stack(s,source),*result=NULL,*target=array(s); assert(target);
    for (unsigned i=0;i<3;++i) assert(xl6ArrayPushV1(source,ARRAY(s),&good));
    if (!strcmp(operation,"clone") || !strcmp(operation,"extract"))
        assert(xl6StackPushBatchV1(owner,STACK(s),source,1001));
    XL6_SequenceAdmissionV3 owned={0}; if (!strcmp(operation,"create")) owned=policy(s,source);
    int64_t count=xl6StackSizeV1(owner,STACK(s),false),capacity=xl6StackSizeV1(owner,STACK(s),true);
    assert(xrtMemDebugFailAfter(point)); bool ok;
    if (!strcmp(operation,"create")) { result=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_STACK_V1,2001,&owned); ok=result!=NULL; }
    else if (!strcmp(operation,"clone")) { result=xl6StackCloneV1(owner,STACK(s)); ok=result!=NULL; }
    else if (!strcmp(operation,"push")) ok=xl6StackPushV1(owner,STACK(s),&good);
    else if (!strcmp(operation,"batch")) ok=xl6StackPushBatchV1(owner,STACK(s),source,1001);
    else { assert(!strcmp(operation,"extract")); ok=xl6StackExtractBatchV1(owner,STACK(s),target,1001,0,3,true); }
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); assert(empty(&owned));
    if (hit) {
        assert(!ok); error(XERR_MEMORY);
        assert(xl6StackSizeV1(owner,STACK(s),false)==count);
        /* XRT push/append may keep a successfully grown capacity if the next
         * element copy fails. Their contract rolls back count/content, not
         * capacity. Clone/extract must leave the source storage unchanged. */
        if (strcmp(operation,"push") && strcmp(operation,"batch"))
            assert(xl6StackSizeV1(owner,STACK(s),true)==capacity);
        for (int64_t i=0;i<count;++i) {
            Slot read={0}; assert(xl6StackTakeV1(owner,STACK(s),i,&read,false)); checkItem(s,&read);
            xrtTypeDropValue(xl6SequenceStorageTypeV1(s->transfer,s->type),&read);
        }
        assert(xl6ArraySizeV1(target,ARRAY(s),false)==0);
    } else {
        assert(ok && !xrtGetError());
        if (result) {
            Slot read={0};
            if (!strcmp(operation,"clone")) { assert(xl6StackTakeV1(result,STACK(s),0,&read,false)); checkItem(s,&read);
                xrtTypeDropValue(xl6SequenceStorageTypeV1(s->transfer,s->type),&read); }
            if (s->profile!=4) {
                Slot bad=item(s,true); assert(!xl6StackPushV1(result,STACK(s),&bad)); error(XERR_TYPE); dropItem(s,&bad);
            }
        }
    }
    xrtValueRelease(result); xrtValueRelease(target); xrtValueRelease(owner); xrtValueRelease(source); dropItem(s,&good);
    balanced(&before); printf("publication fault profile=%u operation=%s point=%zu hit=%u balanced\n",s->profile,operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==4) { Spec s=spec((unsigned)strtoul(argv[1],NULL,10)); fault(&s,argv[2],(size_t)strtoull(argv[3],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (unsigned cycle=0;cycle<100;++cycle) {
        for (unsigned i=0;i<5;++i) { Spec s=spec(i); vectors(&s); }
        Spec s=spec(0); contracts(&s); graph();
    }
    balanced(&before); puts("Sequence publication: 5 profiles, 100 cycles, owned policies/clone/write/batch/graph balanced"); return 0;
}
