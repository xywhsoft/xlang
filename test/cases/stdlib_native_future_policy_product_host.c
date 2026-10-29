/* Exact native-leaf Future policy. Unknown payloads remain unadmitted; no
 * global xvalue-payload family or speculative user callback is necessary. */
#include "xruntime_decl.h"
#include "__xl_runtime/xl6_ownership_domains.h"
#include "__xl_runtime/xl6_native_future_payload.h"
#include "__xl_runtime/xl6_slot_map_key_capture.h"
#include <assert.h>
#include <stdio.h>

static unsigned observations;
static bool observe(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    (void)ref;(void)adapter;(void)preparation;(void)completion;
    assert(policies->PayloadCount==1 && policies->Payload[0]==xl6NativeFuturePayloadRuntimeV1Get()->Policy());
    assert(!policies->ProducerCount && !policies->WatchCount && !policies->HandleCount && !policies->SequenceCount);
    ++observations;return false;
}
static void transaction(void)
{
    const XL6_NativeFuturePayloadRuntimeV1* api=xl6NativeFuturePayloadRuntimeV1Get();
    xfuture* future=NULL;xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise && future);
    void* data=api->Allocate(xl6SlotMapKeyCaptureProfileV1Get());assert(data);
    /* Typed zero is the native key's valid empty carrier. Nonempty keys and
     * optional keys are exercised by the original language AOT fixtures. */
    assert(api->ResolveOwned(promise,data));xrtPromiseDestroy(promise);
    const XL6_OwnershipDomainV1 observer={sizeof(observer),NULL,observe,NULL};
    const XL6_OwnershipDomainV1* missing[]={xl6OwnershipDomainNativeFutureV1Get()};
    const XL6_OwnershipDomainV1* exact[]={missing[0],xl6OwnershipDomainNativeFuturePayloadV1Get(),&observer};
    XL6_OwnershipCatalog* absent=xl6OwnershipCatalogCreateV1(missing,1);assert(absent);
    XL6_OwnershipCatalog* admitted=xl6OwnershipCatalogCreateV1(exact,3);assert(admitted);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref ref=xrtFutureOwnership(future);
    assert(!xl6OwnershipCatalogResolveV1(ref,absent,&adapter,&preparation,&completion));
    assert(!adapter && !preparation && !completion && !xrtGetError());
    assert(xl6OwnershipCatalogResolveV1(ref,admitted,&adapter,&preparation,&completion));
    assert(adapter && observations && !completion && !xrtGetError());
    assert(xrtOwnershipScopeEnd(&freeze));xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount);
    xl6OwnershipCatalogDestroyV1(admitted);xl6OwnershipCatalogDestroyV1(absent);xrtFutureDestroy(future);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));transaction();
    for(unsigned round=0;round<8;++round){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);transaction();xrtMemDebugSnapshot(&after);
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
            before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
            before.UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
    }
    puts("native Future policy: exact admission, missing-policy rejection, frozen no-allocation, 8 balanced lifecycles");return 0;
}
