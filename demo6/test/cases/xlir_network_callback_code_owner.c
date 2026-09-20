/* Include an unmodified, real generated module. These probes exercise its
 * transport result adapters with real native values and a controlled Promise;
 * they are not a network-engine or physical-unmapping acceptance test. */
#define XRT_MODULE_MEMORY_DEBUG
#define main xlir_network_generated_main
#include XLIR_NETWORK_GENERATED_C
#undef main
#include <assert.h>

typedef struct CodeOwner { unsigned pins,acquired,released;bool closed; } CodeOwner;
static int acquire(void* data)
{ CodeOwner* owner=data;if(owner->closed)return 0;++owner->pins;++owner->acquired;return 1; }
static void release(void* data)
{ CodeOwner* owner=data;assert(owner->pins);--owner->pins;++owner->released; }
static bool count(const void* data,size_t* refs)
{ const CodeOwner* owner=data;*refs=owner->pins+!owner->closed;return true; }
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data;(void)visit;(void)context;return true; }
static const xrtownershipops ops={count,trace};
#if defined(XLIR_NETWORK_RESOLVER)
static void addresses_drop(void* value,void* context)
{ (void)context;xrtNetAddrListDestroy(value); }
#endif
int main(void)
{
    xmemdebugsnapshot before,after;CodeOwner owner={0};xfuture *source=NULL,*output;
    XL6_CodeOwnerGraph descriptor={{sizeof(descriptor),&owner,acquire,release},UINT64_C(0x584C36434F574E31),&ops};
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    assert(xl_script_bind_code_owner(&descriptor.base));
    xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
#if defined(XLIR_NETWORK_RESOLVER)
    output=xlir_datagram_future_map(xrtFutureRef(source),xlir_datagram_map_addresses,(void*)(uintptr_t)207);
#else
    output=xl6_net_future_map(xrtFutureRef(source),xl6_net_map_bool);
#endif
    assert(output && xrtFutureState(output)==XFUTURE_PENDING);
    bool pendingHeld=owner.pins==1;owner.closed=true;
#if defined(XLIR_NETWORK_RESOLVER)
    xnetaddr address;assert(xrtNetAddrParse(&address,"127.0.0.1",0));
    xnetaddrlist* addresses=xrtNetAddrListCreate(&address,1);assert(addresses);
    assert(xrtPromiseResolveOwned(promise,addresses,addresses_drop,NULL));
    bool resultHeld=owner.pins==1;
    assert(xrtFutureState(output)==XFUTURE_RESOLVED);
    xvalue** box=xrtFutureValue(output);assert(box && *box);
    xnetaddrlist* mapped=xlir_net_addresses_borrow_handle(*box);
    assert(mapped && xrtNetAddrListCount(mapped)==1 && xrtNetAddrListGet(mapped,0)->Port==207);
#else
    assert(xrtPromiseResolve(promise,NULL));bool resultHeld=owner.pins==0;
    assert(xrtFutureState(output)==XFUTURE_RESOLVED && *(bool*)xrtFutureValue(output));
#endif
    xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);xrtClearError();
    assert(xrtRuntimeRetireThreadStorage());xrtMemDebugSnapshot(&after);
    bool balanced=after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount &&
        after.UseAfterFreeCount==before.UseAfterFreeCount;
    bool complete=pendingHeld && resultHeld && !owner.pins && owner.acquired==1 && owner.released==1 && balanced;
    printf("{\"PendingCodeHeld\":%s,\"ResultCodeContract\":%s,\"Acquired\":%u,\"Released\":%u,\"Balanced\":%s,\"Complete\":%s}\n",
        pendingHeld?"true":"false",resultHeld?"true":"false",owner.acquired,owner.released,balanced?"true":"false",complete?"true":"false");
    return complete?0:1;
}
