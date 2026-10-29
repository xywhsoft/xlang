/* Only configuration, allocation rollback and actual language-owner graphs. */
#include "src/runtime/xl6_tls_stream_options_internal.h"
#include "src/runtime/xl6_tls_stream_options_abi.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
static size_t budgets,failures,publications;
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
}
#define FAULTS(Call) do{bool done=false;for(size_t i=0;i<32;i++){ \
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(i)); \
    void* p=(Call);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();budgets++; \
    assert(hit?(!p && xrtGetError()):(p && !xrtGetError()));if(hit)failures++; \
    xrtClearError();xl6TlsStreamOptionsDestroyV1(p);balanced(before);if(!hit){done=true;break;}}assert(done);}while(0)
static void publication(void)
{
    bool done=false;
    for(size_t i=0;i<32;i++){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* p=xl6TlsStreamOptionsCreateV1();assert(p && xrtMemDebugFailAfter(i));
        xvalue* value=xl6NativeHandleTakeV1(&p,xl6TlsStreamOptionsHandlePolicyV1Get());bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();publications++;
        assert(!p && (hit?(!value && xrtGetError()):(value && !xrtGetError())));xrtClearError();
        if(value){
            const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainTlsStreamOptionsV1Get()};
            XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
            xrtownershipref ref=xrtValueOwnership(value);xrtownershipscope freeze={0};size_t count=0;
            assert(xrtOwnershipFreezeTryBegin(&freeze) && ref.Ops->Count(ref.Data,&count) && count==1);
            const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* finish=NULL;
            assert(xl6OwnershipCatalogResolveV1(ref,catalog,&adapter,&preparation,&finish) && adapter && !preparation && !finish);
            int token=0;assert(adapter->Hold(ref.Data) && adapter->Claim(ref.Data,&token));adapter->Restore(ref.Data,&token);
            assert(xrtOwnershipScopeEnd(&freeze));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);done=true;
        }
        xrtValueRelease(value);balanced(before);if(done)break;
    }assert(done);
}
static void range(void* p){assert(!p && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    void* original=xl6TlsStreamOptionsCreateV1();assert(original);
    const xtlsstreamconfig* raw=original;
    assert(raw->HandshakeTimeout==XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT && raw->CloseTimeout==XTLS_STREAM_CLOSE_TIMEOUT_DEFAULT &&
        raw->AsyncBytesLimit==XTLS_STREAM_ASYNC_BYTES_DEFAULT && raw->AsyncCountLimit==XTLS_STREAM_ASYNC_COUNT_DEFAULT && raw->AsyncBatch==XTLS_STREAM_ASYNC_BATCH_DEFAULT);
    assert(xl6TlsStreamOptionsHandshakeTimeoutV1(original)==10000 && xl6TlsStreamOptionsCloseTimeoutV1(original)==5000);
    FAULTS(xl6TlsStreamOptionsCreateV1());
    FAULTS(xl6TlsStreamOptionsWithTimeoutsV1(original,123,456));
    FAULTS(xl6TlsStreamOptionsWithAsyncLimitsV1(original,1024,2,4));
    const int64_t invalid[]={0,-2,INT64_MAX,(int64_t)(UINT64_MAX/1000)+1};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++){
        range(xl6TlsStreamOptionsWithTimeoutsV1(original,invalid[i],1));range(xl6TlsStreamOptionsWithTimeoutsV1(original,1,invalid[i]));}
    void* limit=xl6TlsStreamOptionsWithTimeoutsV1(original,(int64_t)(UINT64_MAX/1000),-1);assert(limit);
    assert(xl6TlsStreamOptionsHandshakeTimeoutV1(limit)==(int64_t)(UINT64_MAX/1000) && xl6TlsStreamOptionsCloseTimeoutV1(limit)==-1);
    xl6TlsStreamOptionsDestroyV1(limit);
    range(xl6TlsStreamOptionsWithAsyncLimitsV1(original,0,1,1));
    const int64_t counts[]={0,-1,(int64_t)UINT32_MAX+1,INT64_MAX};
    for(size_t i=0;i<sizeof(counts)/sizeof(*counts);i++){
        range(xl6TlsStreamOptionsWithAsyncLimitsV1(original,1,counts[i],1));range(xl6TlsStreamOptionsWithAsyncLimitsV1(original,1,1,counts[i]));}
    limit=xl6TlsStreamOptionsWithAsyncLimitsV1(original,SIZE_MAX,UINT32_MAX,UINT32_MAX);assert(limit);
    assert(xl6TlsStreamOptionsAsyncBytesLimitV1(limit)==SIZE_MAX && xl6TlsStreamOptionsAsyncCountLimitV1(limit)==UINT32_MAX && xl6TlsStreamOptionsAsyncBatchV1(limit)==UINT32_MAX);
    xl6TlsStreamOptionsDestroyV1(limit);
    xtlsstreamconfig rounded=*raw;rounded.HandshakeTimeout=1001;rounded.CloseTimeout=UINT64_MAX;
    limit=xl6TlsStreamOptionsCopyV1(&rounded);assert(limit && xl6TlsStreamOptionsHandshakeTimeoutV1(limit)==2 && xl6TlsStreamOptionsCloseTimeoutV1(limit)==(int64_t)(UINT64_MAX/1000+1));xl6TlsStreamOptionsDestroyV1(limit);
    assert(!xl6TlsStreamOptionsWithTimeoutsV1(NULL,1,1) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    assert(!xl6TlsStreamOptionsCopyV1(NULL) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    rounded.AsyncBatch=0;range(xl6TlsStreamOptionsCopyV1(&rounded));
    xl6TlsFailV1(XERR_ARGUMENT,"preserved incoming error");const xerror* incoming=xrtGetError();
    assert(!xl6TlsStreamOptionsCreateV1() && !xl6TlsStreamOptionsWithTimeoutsV1(original,1,1) && xrtGetError()==incoming);xrtClearError();
    publication();assert(raw->HandshakeTimeout==XTLS_STREAM_HANDSHAKE_TIMEOUT_DEFAULT && raw->AsyncCountLimit==XTLS_STREAM_ASYNC_COUNT_DEFAULT);
    xl6TlsStreamOptionsDestroyV1(original);balanced(before);assert(xrtMemDebugReset());
    printf("TLS stream options owned: %zu budgets; %zu failures; %zu publications; real catalog; immutable copies; balanced\n",budgets,failures,publications);
    return 0;
}
