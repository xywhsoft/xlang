#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_value.h"
#include "src/runtime/xl6_array_iterator.h"
#include "src/runtime/xl6_value_iterator.h"
#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define BASE UINT64_C(8701)
#define OPTIONAL UINT64_C(8702)
#define ID(nullable) ((nullable) ? UINT64_C(8802) : UINT64_C(8801))
#define ELEM(nullable) ((nullable) ? OPTIONAL : BASE)
#define LAYOUT(nullable) ID(nullable),ELEM(nullable),XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1,sizeof(xfuture*),xrtTypeFuture()

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
    assert(!xrtGetError());
}
static xfuture* ready(void)
{
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
    assert(xrtPromiseResolve(promise,NULL)); xrtPromiseDestroy(promise); return future;
}
static xvalue* create(bool nullable) { return xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,ID(nullable),ELEM(nullable),BASE,nullable); }
static xvalue* box(xfuture* future,uint64_t identity)
{ xvalue* value=xrtValueFuture(future); assert(value && xrtValueTypeIdBind(value,identity)); return value; }
static void vectors(bool nullable)
{
    xfuture* future=ready(); xvalue* array=create(nullable); assert(array);
    assert(xl6ArrayPushV1(array,LAYOUT(nullable),&future) && xl6ArrayPushV1(array,LAYOUT(nullable),&future));
    xfuture* absent=NULL; assert(xl6ArrayPushV1(array,LAYOUT(nullable),&absent)==nullable); xrtClearError();
    xfuture* copy=NULL; assert(xl6ArrayReadV1(array,LAYOUT(nullable),0,&copy) && copy==future); xrtFutureDestroy(copy);
    xvalue* alias=xl6ArrayEraseV1(array,LAYOUT(nullable),NULL); assert(alias==array);
    xvalue* good=box(future,BASE),*wrong=box(future,BASE+99),*fake=xrtValueUInt(7); assert(fake && xrtValueTypeIdBind(fake,BASE));
    int64_t count=xl6ArrayErasedSizeV1(alias);
    assert(!xl6ArrayErasedPushV1(alias,wrong) && !xl6ArrayErasedPushV1(alias,fake)); xrtClearError();
    assert(xl6ArrayErasedSizeV1(alias)==count && xl6ArrayErasedPushV1(alias,good));
    xvalue* observed=xl6ArrayErasedReadV1(alias,0); assert(observed && xrtValueIsFuture(observed) &&
        xrtValueGetFuture(observed)==future && xrtValueTypeId(observed)==BASE); xrtValueRelease(observed);
    XL6_ArrayInputV1 input={0}; assert(xl6ArrayInputAcquireV1(array,&input) && input.count==(size_t)count+1);
    assert(xrtValueGetFuture(input.items[0])==future && xrtValueTypeId(input.items[0])==BASE);
    xvalue* snapshot=xl6ArrayToValueV1(array,LAYOUT(nullable),NULL); assert(snapshot &&
        xrtValueGetFuture(xrtValueArrayAt(snapshot,0))==future && xrtValueTypeId(xrtValueArrayAt(snapshot,0))==BASE);
    xvalue* cloned=xl6ArrayCloneV1(array,LAYOUT(nullable)); assert(cloned && xl6ArrayEqualsV1(array,cloned,LAYOUT(nullable)));
    xvalue* cursor=xl6ArrayIteratorCreateV1(array,LAYOUT(nullable),ID(nullable)+50); assert(cursor);
    const XL6_ValueIteratorRuntimeV2* api=xl6ValueIteratorRuntimeV2Get();
    XL6_ValueIterator* language=api->Create(array); assert(language);
    assert(xl6ArrayClearV1(array,LAYOUT(nullable))); xrtFutureDestroy(future);
    for (size_t i=0;i<(size_t)count+1;++i) {
        bool found=false; assert(api->Advance(language,&found) && found);
        const XL6_ValueIteratorView* view=api->View(language);
        assert(view && view->ordinal==i && view->key.Type==XVALUE_KEY_INDEX && view->key.Index==i);
        if (nullable && i==2) assert(view->item==xrtValueNull());
        else assert(xrtValueIsFuture(view->item) && xrtValueGetFuture(view->item)==future &&
            xrtValueTypeId(view->item)==BASE && xrtFutureDone(xrtValueGetFuture(view->item)));
    }
    bool found=true; assert(api->Advance(language,&found) && !found); api->Destroy(language);
    assert(xl6ArrayIteratorAdvanceV1(cursor,ID(nullable)+50,ELEM(nullable),4,sizeof(xfuture*),xrtTypeFuture()));
    copy=NULL; assert(xl6ArrayIteratorReadV1(cursor,ID(nullable)+50,ELEM(nullable),4,sizeof(xfuture*),xrtTypeFuture(),&copy));
    assert(copy && xrtFutureDone(copy)); xrtFutureDestroy(copy);
    assert(xl6ArrayResizeV1(array,LAYOUT(nullable),4)==nullable); xrtClearError();
    if (nullable) { copy=ready(); assert(xl6ArrayReadV1(array,LAYOUT(nullable),0,&copy) && !copy); }
    xrtValueRelease(cursor); xrtValueRelease(cloned); xrtValueRelease(snapshot);
    xl6ArrayInputReleaseV1(&input); xrtValueRelease(fake); xrtValueRelease(wrong); xrtValueRelease(good);
    xrtValueRelease(alias); xrtValueRelease(array);
}
static void payloadDrop(void* payload,void* context) { (void)context; xrtValueRelease(payload); }
static bool payloadTrace(const void* payload,const void* data,xrtownershipvisitor visit,void* context)
{ (void)data; return visit(xrtValueOwnership(payload),context); }
static const xfuturepayloadownershipv1 payloadPolicy={sizeof(payloadPolicy),payloadDrop,payloadTrace};
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    const XL6_SequenceAdmissionOpsV2* sequence[]={xl6ArrayFutureAdmissionOpsV2Get()};
    const xfuturepayloadownershipv1* payloads[]={&payloadPolicy};
    const xrtownershipadapterv1* adapter=context ? xl6SequenceOwnershipAdapterV1(ref) :
        xl6SequenceOwnershipAdapterV2(ref,sequence,1);
    if (!adapter) adapter=xrtFutureOwnershipAdapterV1(ref,payloads,1);
    if (!adapter) adapter=xrtCancelOwnershipAdapterV1(ref);
    if (!adapter) adapter=xrtErrorOwnershipAdapterV1(ref);
    if (!adapter) adapter=xrtValueFutureOwnershipAdapterV1(ref);
    return adapter ? adapter : xrtValueOwnershipAdapterV1(ref);
}
static void graph(bool nullable)
{
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
    xvalue* array=create(nullable); assert(array);
    assert(xl6ArrayPushV1(array,LAYOUT(nullable),&future) && xl6ArrayPushV1(array,LAYOUT(nullable),&future));
    xvalue* weak=xrtValueWeakRef(array),*payload=xrtValueRetain(array); assert(weak && payload);
    assert(xrtPromiseResolveOwnedPolicyV1(promise,payload,&payloadPolicy));
    xrtPromiseDestroy(promise); xrtValueRelease(array); xrtFutureDestroy(future);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtFutureOwnership(future),resolve,(void*)1,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);
    xrtClearError(); assert(!xrtValueWeakRefExpired(weak));
    assert(xrtFutureRef(future));
    assert(xl6OwnershipPlanTryCreate(xrtFutureOwnership(future),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    xrtFutureDestroy(future);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && xrtValueWeakRefExpired(weak));
    xrtValueRelease(weak);
}
static void fault(bool nullable,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xfuture* future=ready(),*output=NULL; xvalue* array=NULL,*alias=NULL,*result=NULL;
    XL6_ArrayInputV1 input={0};
    if (strcmp(operation,"create")) {
        array=create(nullable); assert(array && xl6ArrayPushV1(array,LAYOUT(nullable),&future));
        alias=xl6ArrayEraseV1(array,LAYOUT(nullable),NULL); assert(alias);
    }
    xvalue* good=box(future,BASE);
    const XL6_ValueIteratorRuntimeV2* api=xl6ValueIteratorRuntimeV2Get(); XL6_ValueIterator* language=NULL;
    if (!strcmp(operation,"language-first") || !strcmp(operation,"language-second")) {
        if (!strcmp(operation,"language-second")) assert(xl6ArrayPushV1(array,LAYOUT(nullable),&future));
        language=api->Create(array); assert(language);
        if (!strcmp(operation,"language-second")) { bool found=false; assert(api->Advance(language,&found) && found); }
    }
    XL6_Sequence* owner=array ? xl6SequenceBeginV1(array,LAYOUT(nullable)) : NULL;
    xarray original={0}; if (owner) { original=xl6SequenceStorageV1(owner)->Storage; xl6SequenceEndV1(owner); }
    assert(xrtMemDebugFailAfter(point)); bool ok=false;
    if (!strcmp(operation,"create")) { result=create(nullable); ok=result!=NULL; }
    else if (!strcmp(operation,"language-create")) { language=api->Create(array); ok=language!=NULL; }
    else if (!strcmp(operation,"language-first") || !strcmp(operation,"language-second")) {
        bool found=false; ok=api->Advance(language,&found); assert(found==ok);
    }
    else if (!strcmp(operation,"clone")) { result=xl6ArrayCloneV1(array,LAYOUT(nullable)); ok=result!=NULL; }
    else if (!strcmp(operation,"cursor")) { result=xl6ArrayIteratorCreateV1(array,LAYOUT(nullable),ID(nullable)+50); ok=result!=NULL; }
    else if (!strcmp(operation,"read")) ok=xl6ArrayReadV1(array,LAYOUT(nullable),0,&output);
    else if (!strcmp(operation,"erased-read")) { result=xl6ArrayErasedReadV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"erased-take")) { result=xl6ArrayErasedTakeV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"snapshot")) { result=xl6ArrayToValueV1(array,LAYOUT(nullable),NULL); ok=result!=NULL; }
    else if (!strcmp(operation,"input")) ok=xl6ArrayInputAcquireV1(array,&input);
    else if (!strcmp(operation,"push")) { ok=true; for (size_t i=0;i<80 && ok;++i) ok=xl6ArrayPushV1(array,LAYOUT(nullable),&future); }
    else if (!strcmp(operation,"erased-push")) { ok=true; for (size_t i=0;i<80 && ok;++i) ok=xl6ArrayErasedPushV1(alias,good); }
    else if (!strcmp(operation,"resize")) ok=xl6ArrayResizeV1(array,LAYOUT(nullable),80);
    else abort();
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) assert(!ok && xrtErrorKind(xrtGetError())==XERR_MEMORY);
    else assert(ok && !xrtGetError());
    if (hit && language) {
        xrtClearError(); bool found=true;
        assert(!api->Advance(language,&found) && !found && xrtErrorKind(xrtGetError())==XERR_STATE);
        xrtClearError(); assert(!api->View(language) && xrtErrorKind(xrtGetError())==XERR_STATE);
    }
    if (!ok && array && strcmp(operation,"push") && strcmp(operation,"erased-push")) {
        owner=xl6SequenceBeginV1(array,LAYOUT(nullable)); assert(owner);
        const xarray* current=&xl6SequenceStorageV1(owner)->Storage;
        assert(current->Data==original.Data && current->Count==original.Count && current->Capacity==original.Capacity);
        xl6SequenceEndV1(owner); assert(!output && !input.items && !input.count);
    }
    xrtClearError(); api->Destroy(language); xrtValueRelease(result); xl6ArrayInputReleaseV1(&input); xrtFutureDestroy(output);
    xrtValueRelease(good); xrtValueRelease(alias); xrtValueRelease(array); xrtFutureDestroy(future);
    balanced(&before); printf("future Array fault %s %zu hit=%d balanced\n",operation,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==4) { fault(atoi(argv[1])!=0,argv[2],(size_t)strtoull(argv[3],NULL,10)); return 0; }
    assert(argc==1);
    for (unsigned n=0;n<2;++n) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); vectors(n!=0); balanced(&before);
        for (unsigned i=0;i<100;++i) { xrtMemDebugSnapshot(&before); graph(n!=0); balanced(&before); }
    }
    puts("2 Future Array profiles: native pointer slots, exact Value identity, inputs/cursors and 200 physical cycles balanced");
    return 0;
}
