#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_iterator.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAYOUT UINT64_C(1001),UINT64_C(2001),3,sizeof(xvalue*),xrtTypeValue()
#ifdef XL6_TEST_ARRAY_NOMINAL
static xvalue* create(bool nullable)
{
    /* Caller memory is transient and deliberately mutated immediately after
     * construction; every subsequent owner must use its own identity span. */
    uint64_t tags[]={10,11};
    XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,2001,tags,2};
    xvalue* value=xl6ArrayNominalCreateV2(1001,2001,&schema,nullable,2001);
    tags[0]=12; tags[1]=13; schema.kind=0;
    return value;
}
static xvalue* element(bool good)
{
    xvalue* value=xrtValueObject(); assert(value);
    const xstrview key={"$xlir.class.runtime-type",sizeof("$xlir.class.runtime-type")-1};
    assert(xrtValueObjectSetNew(value,key,xrtValueInt(good ? 11 : 12))); return value;
}
#else
static xvalue* create(bool nullable)
{ return xl6ArrayReferenceCreateV2(1001,2001,XL6_VALUE_ADMIT_BYTES,nullable,2001); }
static xvalue* element(bool good)
{
    xvalue* value=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(value);
    if (!good) assert(xrtValueTypeIdBind(value,2002));
    return value;
}
#endif
static void error(xerrkind kind)
{
    if (!xrtGetError() || xrtErrorKind(xrtGetError())!=kind)
        fprintf(stderr,"expected error kind %d, actual %d\n",(int)kind,
            xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : -1);
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError();
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static void refusal(const xvalue* value,const xvalue* bad)
{
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayPushV1(value,LAYOUT,&bad)); error(XERR_TYPE);
    assert(!xl6ArraySetV1(value,LAYOUT,0,&bad)); error(XERR_TYPE);
    assert(!xl6ArrayInsertV1(value,LAYOUT,0,&bad)); error(XERR_TYPE);
    assert(!xl6ArrayErasedPushV1(value,bad)); error(XERR_TYPE);
    assert(!xl6ArrayErasedSetV1(value,0,bad)); error(XERR_TYPE);
    assert(!xl6ArrayErasedInsertV1(value,0,bad)); error(XERR_TYPE);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6ArraySizeV1(value,LAYOUT,false)==1);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    (void)context; const xrtownershipadapterv1* adapter=xl6SequenceOwnershipAdapterV1(ref);
    return adapter ? adapter : xrtValueOwnershipAdapterV1(ref);
}
static void graph(void)
{
    /* The immutable policy is an owned resource, never an owning Value edge.
     * A profiled owner/cursor cycle must still retire through the real graph. */
#ifdef XL6_TEST_ARRAY_NOMINAL
    uint64_t identities[]={1001,1002};
    XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_EXACT_ID,0,identities,2};
    xvalue* value=xl6ArrayNominalCreateV2(1001,2001,&schema,false,1001);
#else
    xvalue* value=xl6ArrayReferenceCreateV2(1001,2001,XL6_VALUE_ADMIT_ANY,false,0);
#endif
    assert(value && xl6ArrayPushV1(value,LAYOUT,&value));
    xvalue* cursor=xl6ArrayIteratorCreateV1(value,LAYOUT,1002);
    xvalue* weak=xrtValueWeakRef(value),*cursorWeak=xrtValueWeakRef(cursor);
    assert(cursor && weak && cursorWeak && xl6ArrayPushV1(value,LAYOUT,&cursor));
    xrtValueRelease(cursor); xrtValueRelease(value);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(weak) && xrtValueWeakRefExpired(cursorWeak));
    xrtValueRelease(weak); xrtValueRelease(cursorWeak);
}
static void malformed(void)
{
    /* Reject malformed descriptions before allocation, not halfway through
     * publishing an owner. Exact IDs span all uint64; runtime tags do not. */
    assert(xrtMemDebugFailAfter(0));
#ifdef XL6_TEST_ARRAY_NOMINAL
    uint64_t identity=0;
    XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,0,&identity,1};
    assert(!xl6ArrayNominalCreateV2(1001,2001,&schema,false,2001)); error(XERR_ARGUMENT);
    identity=UINT64_MAX;
    assert(!xl6ArrayNominalCreateV2(1001,2001,&schema,false,2001)); error(XERR_ARGUMENT);
    identity=11; schema.count=SIZE_MAX;
    assert(!xl6ArrayNominalCreateV2(1001,2001,&schema,false,2001)); error(XERR_ARGUMENT);
    schema.count=1; schema.identities=NULL;
    assert(!xl6ArrayNominalCreateV2(1001,2001,&schema,false,2001)); error(XERR_ARGUMENT);
    schema.identities=&identity; schema.nonObjectIdentity=2002;
    assert(!xl6ArrayNominalCreateV2(1001,2001,&schema,false,2001)); error(XERR_ARGUMENT);
    schema.kind=XL6_NOMINAL_ADMIT_PROTOCOL_TAG; schema.nonObjectIdentity=2001;
    assert(!xl6ArrayNominalCreateV2(1001,2001,&schema,false,2001)); error(XERR_ARGUMENT);
#else
    assert(!xl6ArrayReferenceCreateV2(1001,2001,XL6_VALUE_ADMIT_I64,false,2001)); error(XERR_ARGUMENT);
    assert(!xl6ArrayReferenceCreateV2(1001,2001,XL6_VALUE_ADMIT_BYTES,false,0)); error(XERR_ARGUMENT);
    assert(!xl6ArrayReferenceCreateV2(0,2001,XL6_VALUE_ADMIT_ANY,false,0)); error(XERR_ARGUMENT);
#endif
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
#ifdef XL6_TEST_ARRAY_NOMINAL
    /* Empty closed protocol domains remain meaningful; nullable admits only
     * Null. Deep copy that schema even though its identity span is empty. */
    XL6_NominalAdmissionV1 empty={XL6_NOMINAL_ADMIT_PROTOCOL_TAG,0,NULL,0};
    xvalue* value=xl6ArrayNominalCreateV2(1001,2001,&empty,true,2001);
    xvalue* good=element(true),*null=xrtValueNull();
    assert(value && xl6ArrayPushV1(value,LAYOUT,&null));
    xvalue* alias=xl6ArrayEraseV1(value,LAYOUT,NULL); assert(alias);
    refusal(alias,good); xrtValueRelease(alias); xrtValueRelease(good); xrtValueRelease(value);
#endif
}
static void vectors(void)
{
    xvalue* value=create(false),*good=element(true),*bad=element(false); assert(value);
    assert(xl6ArrayPushV1(value,LAYOUT,&good));
    xvalue* alias=xl6ArrayEraseV1(value,LAYOUT,NULL); assert(alias==value);
    refusal(alias,bad);
    assert(!xl6ArrayResizeV1(alias,LAYOUT,2)); error(XERR_TYPE);
    assert(xl6ArraySizeV1(alias,LAYOUT,false)==1);
    xvalue* copy=xl6ArrayCloneV1(alias,LAYOUT); assert(copy);
    assert(xl6ArrayEraseV1(copy,LAYOUT,NULL)==copy); xrtValueRelease(copy);
    xvalue* erasedCopy=xl6ArrayErasedCloneV1(alias); assert(erasedCopy);
    xvalue* cursor=xl6ArrayIteratorCreateV1(alias,LAYOUT,1002); assert(cursor);
    xrtValueRelease(value); xrtValueRelease(alias);
    refusal(copy,bad); refusal(erasedCopy,bad);
    assert(xl6ArrayIteratorAdvanceV1(cursor,1002,2001,3,sizeof(xvalue*),xrtTypeValue()));
    xvalue* observed=NULL;
    assert(xl6ArrayIteratorReadV1(cursor,1002,2001,3,sizeof(xvalue*),xrtTypeValue(),&observed));
    assert(observed==good); xrtValueRelease(observed);
    /* Same physical tuple but missing semantic policy must not poison a
     * profiled owner through bulk append/equality. */
    value=xl6ArrayCreateV1(LAYOUT); assert(value && xl6ArrayPushV1(value,LAYOUT,&bad));
    assert(!xl6ArrayAppendV1(copy,value,LAYOUT)); error(XERR_TYPE);
    assert(!xl6ArrayEqualsV1(copy,value,LAYOUT)); error(XERR_TYPE);
    assert(xl6ArrayAppendV1(copy,erasedCopy,LAYOUT) && xl6ArraySizeV1(copy,LAYOUT,false)==2);
    assert(xl6ArrayAppendV1(copy,copy,LAYOUT) && xl6ArraySizeV1(copy,LAYOUT,false)==4);
    /* Reject a policy copied onto another physical carrier before invoking
     * its pointer-slot predicate (integer bits are not a Value address). */
    XL6_SequenceAdmissionV1 policy={0};
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(copy,XL6_SEQUENCE_ARRAY_V1,LAYOUT); assert(owner);
    assert(xl6SequenceAdmissionCopyV1(owner,&policy)); xl6SequenceEndV1(owner);
    xvalue* scalar=xl6ArrayCreateV1(1003,2001,1,sizeof(int64_t),xrtTypeInt64()); assert(scalar);
    int64_t bits=INT64_MAX; assert(xl6ArrayPushV1(scalar,1003,2001,1,sizeof(int64_t),xrtTypeInt64(),&bits));
    owner=xl6SequenceBeginRoleV1(scalar,XL6_SEQUENCE_ARRAY_V1,1003,2001,1,sizeof(int64_t),xrtTypeInt64());
    assert(owner && !xl6SequenceAdmissionTakeV1(owner,&policy)); error(XERR_ARGUMENT);
    assert(!policy.ops && !policy.data); xl6SequenceEndV1(owner); xrtValueRelease(scalar);
    xrtValueRelease(value); xrtValueRelease(copy); xrtValueRelease(erasedCopy); xrtValueRelease(cursor);
    value=create(true); assert(value && xl6ArrayResizeV1(value,LAYOUT,2));
    alias=xl6ArrayEraseV1(value,LAYOUT,NULL); assert(alias);
    observed=xl6ArrayErasedReadV1(alias,0); assert(observed && xrtValueType(observed)==XVALUE_NULL);
    xrtValueRelease(observed); assert(xl6ArrayErasedSetV1(alias,0,good));
    assert(!xl6ArrayErasedSetV1(alias,1,bad)); error(XERR_TYPE);
    assert(xl6ArrayResizeV1(alias,LAYOUT,0)); xrtValueRelease(alias); xrtValueRelease(value);
    xrtValueRelease(good); xrtValueRelease(bad);
    /* nullable<T>'s owner identity differs from T's boxed admission identity. */
    good=xrtValueBytes((xbytesview){(const uint8_t*)"a",1});
    assert(good && xrtValueTypeIdBind(good,2001));
#ifdef XL6_TEST_ARRAY_NOMINAL
    uint64_t tags[]={10}; XL6_NominalAdmissionV1 schema={XL6_NOMINAL_ADMIT_CLASS_TAG,2001,tags,1};
    value=xl6ArrayNominalCreateV2(1101,2101,&schema,true,2001);
#else
    value=xl6ArrayReferenceCreateV2(1101,2101,XL6_VALUE_ADMIT_BYTES,true,2001);
#endif
    assert(value && xl6ArrayPushV1(value,1101,2101,3,sizeof(xvalue*),xrtTypeValue(),&good));
    alias=xl6ArrayEraseV1(value,1101,2101,3,sizeof(xvalue*),xrtTypeValue(),NULL); assert(alias);
    assert(xl6ArrayErasedPushV1(alias,xrtValueNull()));
    xrtValueRelease(alias); xrtValueRelease(value); xrtValueRelease(good);
}
static void fault(const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* good=element(true),*value=NULL,*alias=NULL,*result=NULL;
    if (strcmp(operation,"create")) {
        value=create(false); assert(value && xl6ArrayPushV1(value,LAYOUT,&good));
        alias=xl6ArrayEraseV1(value,LAYOUT,NULL); assert(alias);
    }
    assert(xrtMemDebugFailAfter(point));
    if (!strcmp(operation,"create")) result=create(false);
    else if (!strcmp(operation,"clone")) result=xl6ArrayCloneV1(value,LAYOUT);
    else if (!strcmp(operation,"erased-clone")) result=xl6ArrayErasedCloneV1(alias);
    else { assert(!strcmp(operation,"cursor")); result=xl6ArrayIteratorCreateV1(value,LAYOUT,1002); }
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); } else assert(result && !xrtGetError());
    xrtValueRelease(result); xrtValueRelease(value); xrtValueRelease(alias); xrtValueRelease(good);
    balanced(&before); printf("array-admission %s point=%zu hit=%d balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) { fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    vectors(); malformed(); graph(); balanced(&before); puts("array-admission vectors balanced"); return 0;
}
