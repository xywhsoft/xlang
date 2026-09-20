#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_future_callback.h"
#include <assert.h>
#include <stdio.h>

/* The owner owns a real Promise endpoint; the frame's real code acquisition
 * owns this same owner. Closing returns the external owner handle, not a
 * subtraction from Count. This exposes the cycle with ONLY an output observer. */
typedef struct Owner {
    size_t references;
    xpromise* promise;
    unsigned acquisitions, releases;
    bool closed;
} Owner;
typedef struct Observation { xfuture* source; unsigned calls, cleanups; } Observation;
static Observation observation; /* Borrowed test coordination, never a graph root. */
static const XL6_FutureCallbackRuntimeV1* api;
static const XL6_FutureCallbackRuntimeV2* api2;
static void begin(xrtownershipscope* scope) { assert(xrtOwnershipMutationBegin(scope)); }
static void end(xrtownershipscope* scope) { assert(xrtOwnershipScopeEnd(scope)); }
static int acquire(void* data)
{
    Owner* owner=data; xrtownershipscope mutation={0}; begin(&mutation);
    if(owner->closed){end(&mutation);return 0;}
    assert(owner->references); ++owner->references; ++owner->acquisitions; end(&mutation); return 1;
}
static void release(void* data)
{
    Owner* owner=data; xpromise* promise=NULL; xrtownershipscope mutation={0}; begin(&mutation);
    assert(owner->references); ++owner->releases;
    if(!--owner->references){promise=owner->promise;owner->promise=NULL;}
    end(&mutation); xrtPromiseDestroy(promise);
}
static bool owner_count(const void* data,size_t* count)
{ const Owner* owner=data;if(!owner->references)return false;*count=owner->references;return true; }
static bool owner_trace(const void* data,xrtownershipvisitor visit,void* context)
{ return visit(xrtPromiseOwnership(((const Owner*)data)->promise),context); }
static const xrtownershipops owner_ops={owner_count,owner_trace};
static const XL6_CaptureSlot slot={XL6_CAPTURE_TRIVIAL,0,sizeof(uint64)};
static const XL6_CaptureLayoutV1 layout={sizeof(layout),sizeof(uint64),1,&slot};
static void callback(const xfutureresult* input,xpromise* output,xcancel* cancel,void* data)
{
    assert(input && *(uint64*)data==182 && !xrtCancelRequested(cancel));
    ++observation.calls;
    api2->Cleanup(data); ++observation.cleanups;
    assert(xrtPromiseForward(output,observation.source));
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void inspect(xrtownershipref output,xrtownershipref frame,Owner* owner,bool observed)
{
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipresult graph={0};xrtownershipsnapshot* snapshot=NULL;
    assert(xrtOwnershipInspect(&output,1,NULL,0,&graph));
    assert(graph.NodeCount==7 && graph.EdgeCount==10 && graph.ExternalRootCount==(observed?1u:0u));
    assert(xrtOwnershipSnapshotCreate(&output,1,NULL,0,NULL,NULL,&snapshot));
    size_t frames=0,owners=0,results=0;
    for(size_t i=0;i<xrtOwnershipSnapshotNodeCount(snapshot);++i){
        xrtownershipnode node;assert(xrtOwnershipSnapshotNode(snapshot,i,&node));
        assert(node.Reachable==observed);
        if(node.Reference.Data==frame.Data){
            assert(node.Reference.Ops==frame.Ops && node.StrongCount==2 && node.InternalCount==2);++frames;
        }else if(node.Reference.Data==owner){
            assert(node.Reference.Ops==&owner_ops && node.StrongCount==1 && node.InternalCount==1);++owners;
        }else if(node.Reference.Data==output.Data){
            assert(node.StrongCount==(observed?2u:1u) && node.InternalCount==1);++results;
        }else assert(node.StrongCount==node.InternalCount);
    }
    assert(frames==1 && owners==1 && results==1);xrtOwnershipSnapshotDestroy(snapshot);
    const xfutureproducerownershipv1* policies[]={xl6FutureCallbackProducerPolicyV1Get()};
    assert(!xrtFutureOwnershipAdapterV1(output,NULL,0));
    assert(xrtFutureOwnershipAdapterV2(output,NULL,0,policies,1));
    /* Inspection is NOT whole graph collection. Registered Watch still needs
     * its semantic completion protocol and independently admitted frame. */
    assert(!xrtFutureOwnershipAdapterV2(xrtFutureOwnership(observation.source),NULL,0,policies,1));
    end(&freeze);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    api=xl6FutureCallbackRuntimeV1Get();api2=xl6FutureCallbackRuntimeV2Get();
    unsigned calls=0,kept=0,dropped=0;
    for(unsigned n=0;n<100;++n)for(unsigned keep=0;keep<2;++keep){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner owner={0};owner.references=1;
        xfuture *source=NULL,*output=NULL;owner.promise=xrtPromiseCreate(&source,NULL);assert(owner.promise);
        observation=(Observation){source,0,0};
        XL6_CodeOwnerGraph code={{sizeof(code),&owner,acquire,release},0x584C36434F574E31ULL,&owner_ops};
        uint64* payload=api2->Allocate(&code.base,sizeof(*payload),callback,&layout);assert(payload);*payload=182;
        xrtownershipref frame=api->Ownership(payload);output=api->Start(payload,source);assert(output);
        xrtownershipref result=xrtFutureOwnership(output);
        xrtFutureDestroy(source); /* The frame and real owner now own source. */
        xrtownershipscope mutation={0};begin(&mutation);owner.closed=true;end(&mutation);release(&owner);
        assert(owner.references==1 && owner.acquisitions==1 && owner.releases==1);
        inspect(result,frame,&owner,true);
        if(!keep){
            xrtFutureDestroy(output);output=NULL;inspect(result,frame,&owner,false);++dropped;
        }else ++kept;
        assert(!observation.calls && !observation.cleanups && !xrtFutureDone(source));
        /* Only AFTER the root proof, acquire a real completion endpoint.
         * No guard lease, fake internal slot, native root or count adjustment. */
        xpromise* completion=xrtPromiseRef(owner.promise);assert(completion);
        if(n%4==0)assert(xrtPromiseResolve(completion,NULL));
        else if(n%4==1){xrtSetErrorKind(XERR_STATE);assert(xrtPromiseReject(completion,xrtGetError()));xrtClearError();}
        else if(n%4==2)assert(xrtPromiseCancel(completion));else assert(xrtPromiseClose(completion));
        assert(observation.calls==1 && observation.cleanups==1 && !owner.references && !owner.promise && owner.releases==2);
        if(output)assert(xrtFutureState(output)==(n%4==0?XFUTURE_RESOLVED:n%4==1?XFUTURE_FAILED:n%4==2?XFUTURE_CANCELLED:XFUTURE_CLOSED));
        xrtPromiseDestroy(completion);xrtFutureDestroy(output);++calls;observation=(Observation){0};balance(&before);
    }
    assert(calls==200 && kept==100 && dropped==100);
    puts("Future producer roots: 200 output-only root graphs, 100 unobserved rootless cycles, 200 accepted callbacks and cleanups across four outcomes, exact physical edges and zero live delta; no guard leases or native roots, not pending graph collection");
    return 0;
}
