#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_FUTURE
#define XRT_MODULE_NET_DNS
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_code_lease.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef struct Owner { unsigned pins,acquired,released,unmapped;bool closed;HMODULE library;void* address; } Owner;
static int acquire(void* data)
{ Owner* owner=data;if(owner->closed)return 0;++owner->pins;++owner->acquired;return 1; }
static void release(void* data)
{
    Owner* owner=data;assert(owner->pins);--owner->pins;++owner->released;
    if(owner->closed && !owner->pins){
        MEMORY_BASIC_INFORMATION memory;assert(!owner->unmapped && FreeLibrary(owner->library));
        assert(VirtualQuery(owner->address,&memory,sizeof(memory))==sizeof(memory) && memory.State==MEM_FREE);
        ++owner->unmapped;
    }
}
static bool count(const void* data,size_t* refs)
{ const Owner* owner=data;*refs=owner->pins+!owner->closed;return true; }
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data;(void)visit;(void)context;return true; }
static const xrtownershipops ops={count,trace};
static void native_drop(void* value,void* context)
{ (void)context;xrtNetAddrListDestroy(value); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();assert(xrtRuntimeRetireThreadStorage());xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && after.OverflowCount==before->OverflowCount &&
        after.UnderflowCount==before->UnderflowCount);
}
int main(int argc,char** argv)
{
    assert(argc==3 && xrtMemDebugEnable(true));bool addresses=!strcmp(argv[2],"addresses");
    unsigned retained=0;
    for(unsigned round=0;round<100;++round){
        xmemdebugsnapshot before;Owner owner={0};xfuture *source=NULL,*output;
        int (*bind)(const XL6_CodeOwner*);xfuture* (*map)(xfuture*);
        xrtMemDebugSnapshot(&before);owner.library=LoadLibraryA(argv[1]);assert(owner.library);
        FARPROC symbol=GetProcAddress(owner.library,"xl_script_bind_code_owner");memcpy(&bind,&symbol,sizeof(bind));
        symbol=GetProcAddress(owner.library,"xlir_network_probe_map");memcpy(&map,&symbol,sizeof(map));owner.address=(void*)symbol;
        assert(bind && map);
        XL6_CodeOwnerGraph descriptor={{sizeof(descriptor),&owner,acquire,release},UINT64_C(0x584C36434F574E31),&ops};
        assert(bind(&descriptor.base));xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise);
        output=map(source);assert(output && owner.pins==1 && owner.acquired==1 && !owner.released);
        owner.closed=true;assert(xrtFutureState(output)==XFUTURE_PENDING);
        if(round%4==0){
            if(addresses){
                xnetaddr address;assert(xrtNetAddrParse(&address,"127.0.0.1",0));
                xnetaddrlist* list=xrtNetAddrListCreate(&address,1);assert(list);
                assert(xrtPromiseResolveOwned(promise,list,native_drop,NULL));
            }else assert(xrtPromiseResolve(promise,NULL));
        }else if(round%4==1){xrtSetErrorKind(XERR_STATE);assert(xrtPromiseReject(promise,xrtGetError()));xrtClearError();}
        else if(round%4==2)assert(xrtPromiseCancel(promise));else assert(xrtPromiseClose(promise));
        assert(xrtFutureState(output)==(round%4==0?XFUTURE_RESOLVED:round%4==1?XFUTURE_FAILED:round%4==2?XFUTURE_CANCELLED:XFUTURE_CLOSED));
        if(addresses && round%4==0){
            assert(owner.pins==1 && !owner.unmapped);xvalue** box=xrtFutureValue(output);assert(box && *box);
            xvalue* kept=xrtValueRetain(*box);assert(kept);xrtFutureDestroy(output);output=NULL;
            assert(owner.pins==1 && !owner.unmapped);xrtValueRelease(kept);++retained;
        }
        assert(owner.pins==0 && owner.released==1 && owner.unmapped==1);
        /* The result Future and its resident payload may outlive mapped code. */
        if(!addresses && round%4==0)assert(*(bool*)xrtFutureValue(output));
        xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
    }
    assert(retained==(addresses?25u:0u));
    printf("Generated native mapper DLL: 100 real unloads after Close, four terminal outcomes, %u independently retained class results, MEM_FREE and exact memory balance\n",retained);
    return 0;
}
