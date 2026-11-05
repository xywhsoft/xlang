/* CPU boundary test. Native publication is a synchronous fake; real leases
 * and resident service construction run. No window, clipboard or OLE call. */
#include "src/common/xl6_xrt.h"
#include <xge.h>
#include <xui.h>
#include <xge3_runtime.h>
#include "src/runtime/xl6_code_lease.h"
#include "src/runtime/xnative_transfer.h"
#include <assert.h>
#include <stdio.h>

/* Include the actual adapter after the normal static Native declarations so
 * the test can define its four backend targets without import-library glue. */
#include "lib/bindings/_xge_native/src/transfer.c"

typedef struct Owner {unsigned refs,acquires,releases;bool closed;} Owner;
static int acquire(void* raw)
{Owner* owner=raw;if(owner->closed)return 0;++owner->refs;++owner->acquires;return 1;}
static void release(void* raw)
{Owner* owner=raw;assert(owner->refs);--owner->refs;++owner->releases;}
static bool resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** prepare,
    const XL6_OwnershipFinishV1** finish)
{(void)ref;(void)policies;*adapter=NULL;*prepare=NULL;*finish=NULL;return false;}
static bool admits(xrtownershipref ref){(void)ref;return false;}
static const XL6_OwnershipDomainV1 transfer={sizeof(transfer),NULL,resolve,admits};
static const XL6_OwnershipDomainV1 ui={sizeof(ui),NULL,resolve,admits};
const XL6_OwnershipDomainV1* xl6Xge3TransferDomainV1Get(void){return &transfer;}
const XL6_OwnershipDomainV1* xl6Xui3OwnershipDomainV1Get(void){return &ui;}

static unsigned calls;
static int nativeResult;
static bool retainExport;
static const XL6_OwnershipDomainV1* expectedDomain;
static XL6_CodeLease* exported;
static int consume(const xn_transfer_services_v1* services)
{
    ++calls;assert(services && services->iSize==sizeof(*services));
    assert(services->pOwnershipDomain==expectedDomain);
    assert(services->wrapDataObject && services->wrapDropSource && services->wrapDropTarget);
    if(services->pCodeCapability){
        assert(!services->iFlags);
        if(retainExport){assert(!exported);exported=xl6CodeLeaseRetain((XL6_CodeLease*)services->pCodeCapability);}
    }else assert(services->iFlags==XN_TRANSFER_PROCESS_RESIDENT);
    return nativeResult;
}
int xgeRunOwnedWithTransfer(xge_scene_proc frame,void* user,const xge_callback_owner_t* owner,
    const xn_transfer_services_v1* services)
{assert(!frame && user==(void*)1 && !owner);return consume(services);}
int xgeDragBeginWithTransfer(xge_data_object data,uint32_t allowed,uint32_t suggested,
    const xn_transfer_services_v1* services)
{assert(data==(xge_data_object)1 && allowed==3 && suggested==1);return consume(services);}
int xgeDataObjectCopyToClipboardWithTransfer(xge_data_object data,const xn_transfer_services_v1* services)
{assert(data==(xge_data_object)1);return consume(services);}
int xuiClipboardCopyDataWithTransfer(xui_context context,xui_data_object data,
    const xn_transfer_services_v1* services)
{assert(context==(xui_context)2 && data==(xui_data_object)1);return consume(services);}
static int invoke(unsigned index,const XL6_CodeOwner* code)
{
    expectedDomain=index==3 ? &ui:&transfer;
    switch(index){
    case 0:return xl6Xge3RunOwned(NULL,(void*)1,NULL,code);
    case 1:return xl6Xge3DragBegin((xge_data_object)1,3,1,code);
    case 2:return xl6Xge3CopyToClipboard((xge_data_object)1,code);
    default:return xl6Xui3CopyToClipboard((xui_context)2,(xui_data_object)1,code);
    }
}
int main(void)
{
    for(unsigned i=0;i<4;++i){
        unsigned before=calls;nativeResult=0;retainExport=false;
        assert(!invoke(i,NULL) && calls==before+1);
        Owner owner={0};XL6_CodeOwner code={sizeof(code),&owner,acquire,release};
        retainExport=true;assert(!invoke(i,&code) && exported && owner.refs==1);
        assert(owner.acquires==1 && !owner.releases);
        owner.closed=true;before=calls;retainExport=false;
        assert(invoke(i,&code)==(i==3 ? XUI_ERROR_INVALID_STATE:XGE_ERROR_INVALID_STATE));
        assert(calls==before && owner.acquires==1);xrtClearError();
        XL6_CodeScope scope={0};xl6CodeScopeEnter(&scope,exported);
        nativeResult=-77;assert(invoke(i,&code)==-77 && calls==before+1);
        assert(owner.acquires==1 && owner.refs==1);xl6CodeScopeLeave(&scope);
        xl6CodeLeaseRelease(exported);exported=NULL;
        assert(!owner.refs && owner.acquires==owner.releases);
        Owner fresh={0};code.context=&fresh;nativeResult=0;before=calls;
        assert(xrtMemDebugFailAfter(0));
        int status=invoke(i,&code);xrtMemDebugFailClear();
        assert(status==(i==3 ? XUI_ERROR_OUT_OF_MEMORY:XGE_ERROR_OUT_OF_MEMORY));
        assert(calls==before && !fresh.refs && fresh.acquires==fresh.releases);
        xrtClearError();
        nativeResult=-91;assert(invoke(i,NULL)==-91);assert(!xrtGetError());
    }
    puts("four cold boundaries: static, retained lease, Close, accepted continuation, OOM, status");
    return 0;
}
