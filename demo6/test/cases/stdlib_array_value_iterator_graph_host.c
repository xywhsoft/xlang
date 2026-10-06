#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_value_iterator.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LAYOUT UINT64_C(18001),UINT64_C(18002),3,sizeof(xvalue*),xrtTypeValue()
static const XL6_ValueIteratorRuntimeV2* api;
static unsigned actualRefusals;
typedef struct Context { XL6_ValueIterator* iterator; const xvalue* snapshot; xvalue* extra; unsigned mode,finalized,dropped; } Context;
static bool emptyTrace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data; (void)visit; (void)context; return true; }
static void emptyDrop(void* data) { assert(!data); }
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const Context* owner=data;
    return !owner->iterator || visit(api->Ownership(owner->iterator),context);
}
static void drop(void* data)
{
    Context* owner=data; assert(owner->finalized==1 && !owner->dropped && !owner->extra);
    xrtownershipscope mutation={0}; assert(xrtOwnershipMutationBegin(&mutation));
    XL6_ValueIterator* iterator=owner->iterator; owner->iterator=NULL; owner->dropped=1;
    assert(xrtOwnershipScopeEnd(&mutation)); api->Destroy(iterator);
    /* Stats stay on the caller stack, not a secretly counted owner edge. */
}
static bool finalizer(xvalue* object,void* data)
{
    Context* owner=data; assert(!owner->finalized && !owner->dropped); ++owner->finalized;
    const XL6_ValueIteratorView* view=api->View(owner->iterator);
    assert(view && view->item==object && view->ordinal==0);
    if (owner->mode==1) { xrtSetErrorKind(XERR_STATE); return false; }
    if (owner->mode==2) {
        /* XRT intentionally forbids retaining a FINALIZING object. Resurrect
         * the independently owned native snapshot, not that object shell. */
        assert(!xrtValueRetain(view->item) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
        owner->extra=xrtValueRetain(owner->snapshot); assert(owner->extra==owner->snapshot);
    }
    return true;
}
static void terminal(xvalue* object,void* data) { (void)finalizer(object,data); }
static const xvalueobjectownershipv1 policy={sizeof(policy),terminal,trace,drop,emptyTrace,emptyDrop,finalizer};
static bool resolve(xrtownershipref ref,void* denied,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation)
{
    *preparation=NULL;
    *adapter=xl6ValueIteratorOwnershipAdapterV1(ref);
    if (*adapter) return true;
    *adapter=xl6SequenceOwnershipAdapterV1(ref);
    if (*adapter) return denied==NULL;
    *adapter=xrtValueObjectOwnershipAdapterV1(ref,&policy);
    if (!*adapter) *adapter=xrtValueOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
static bool snapshotChild(xrtownershipref ref,void* data)
{
    Context* context=data;
    if (!context->snapshot) { assert(xl6SequenceOwnershipAdapterV1(ref)); context->snapshot=ref.Data; }
    else assert(ref.Data!=context->snapshot);
    return true;
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static void cycle(unsigned mode)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); Context context={.mode=mode};
    xvalue* object=xrtValueObject(); assert(object &&
        xrtValueObjectLifetimeBindOwned(object,NULL,emptyTrace,emptyDrop) &&
        xrtValueObjectFinalizerPrepareOwned(object,terminal,&context,trace,drop) &&
        xrtValueObjectOwnershipBindV1(object,&policy) && xrtValueObjectFinalizerCommit(object));
    xvalue* array=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,18001,18002,XL6_VALUE_ADMIT_ANY,false,0);
    assert(array && xl6ArrayPushV1(array,LAYOUT,&object) && xl6ArrayPushV1(array,LAYOUT,&object));
    xvalue* alias=xl6ArrayEraseV1(array,LAYOUT,NULL); assert(alias==array);
    XL6_ValueIterator* iterator=api->Create(array); bool found=false;
    assert(iterator && api->Advance(iterator,&found) && found);
    xrtownershipscope mutation={0}; assert(xrtOwnershipMutationBegin(&mutation));
    context.iterator=iterator; assert(xrtOwnershipScopeEnd(&mutation));
    xvalue* weak=xrtValueWeakRef(object); assert(weak);
    xrtValueRelease(alias); xrtValueRelease(array);
    xrtownershipref root=api->Ownership(context.iterator); XL6_OwnershipPlan* plan=NULL;
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(root.Ops->Trace(root.Data,snapshotChild,&context) && context.snapshot);
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED && plan);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); plan=NULL;
    xrtValueRelease(object);
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,&context,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);
    xrtClearError(); assert(!context.finalized && !context.dropped);
    bool closed=false;
    for (size_t point=0;point<32;++point) {
        xmemdebugsnapshot scratch; xrtMemDebugSnapshot(&scratch); assert(xrtMemDebugFailAfter(point));
        XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (hit) { assert(status==XL6_OWNERSHIP_PLAN_ERROR && !plan); xrtClearError(); ++actualRefusals; }
        else { closed=true; assert(status==XL6_OWNERSHIP_PLAN_FINALIZERS && plan); assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); plan=NULL; }
        balance(&scratch);
    }
    assert(closed);
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS && plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    XL6_OwnershipStepResult status=xl6OwnershipPlanFinalizeV2(plan);
    assert(context.finalized==1 && !context.dropped);
    if (mode==1) {
        assert(status==XL6_OWNERSHIP_STEP_FAILED && xrtErrorKind(xrtGetError())==XERR_STATE);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); plan=NULL; xrtClearError();
        assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS && plan);
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY && context.finalized==1);
    } else assert(status==XL6_OWNERSHIP_STEP_READY);
    if (mode==2) {
        assert(context.extra && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); plan=NULL;
        xrtValueRelease(context.extra); context.extra=NULL;
        assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS && plan);
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY && context.finalized==1);
    }
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && !context.dropped);
    assert(xl6OwnershipPlanFinish(plan) && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(context.dropped==1 && xrtValueWeakRefExpired(weak)); xrtValueRelease(weak); balance(&before);
}
static void bytesVectors(void)
{
    xvalue* array=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,18001,18002,XL6_VALUE_ADMIT_BYTES,true,18002);
    xvalue* item=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(array && item);
    assert(xrtValueTypeIdBind(item,18002) && xl6ArrayPushV1(array,LAYOUT,&item));
    xvalue* null=xrtValueNull(); assert(xl6ArrayPushV1(array,LAYOUT,&null));
    xvalue* alias=xl6ArrayEraseV1(array,LAYOUT,NULL); assert(alias==array);
    XL6_ValueIterator* iterator=api->Create(array); assert(iterator);
    xrtValueRelease(alias); xrtValueRelease(array);
    assert(xrtMemDebugFailAfter(0)); bool found=false;
    assert(api->Advance(iterator,&found) && found && api->View(iterator)->item==item && !xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); xbytesview view={0};
    assert(xrtValueGetBytes(api->View(iterator)->item,&view) && view.Size==3 && !memcmp(view.Data,"a\0b",3));
    xrtValueRelease(item);
    assert(api->Advance(iterator,&found) && found && api->View(iterator)->item==null);
    assert(api->Advance(iterator,&found) && !found); api->Destroy(iterator);
}
int main(void)
{
    api=xl6ValueIteratorRuntimeV2Get(); assert(xrtMemDebugEnable(true)); xrtSetErrorKind(XERR_MEMORY); xrtClearError();
    for (unsigned round=0;round<25;++round) {
        for (unsigned mode=0;mode<3;++mode) cycle(mode);
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); bytesVectors(); balance(&before);
    }
    assert(actualRefusals);
    printf("Native iterator graph: 75 complete transactions, 2400 OOM budgets, %u actual refusals, finalizer retry/resurrection, bytes/NUL/null and exact balance\n",actualRefusals);
    return 0;
}
