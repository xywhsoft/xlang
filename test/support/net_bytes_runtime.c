/* Exact resident closure; no Engine, socket, task or network I/O. */
#include "net-bytes-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#define XL6_TEXT_API
#include "src/runtime/xl6_net_bytes_abi.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static XL6_OwnershipCatalog* catalog;
static bool admit(xrtownershipref ref,void* context)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;
    const XL6_OwnershipFinishV1* finish=NULL;(void)context;
    return xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&prepare,&finish)==XL6_OWNERSHIP_ADMISSION_ADMITTED;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static void errorEnd(xl6_error_scope* scope,xerrkind kind)
{xl6_error_result result=xl6ErrorScopeEnd(scope);assert(!result.ok&&(!kind||xrtErrorKind(result.error)==kind));xrtErrorFree(result.error);}
static void graph(void* raw)
{
    xnetbytes* native=raw;const XL6_NativeHandlePolicyV1* policy=xl6NetBytesHandlePolicyV1Get();
    xvalue* handle=xl6NativeHandleTakeV1(&raw,policy);assert(handle&&!raw);
    xrtownershipscope frozen={0};assert(xrtOwnershipFreezeTryBegin(&frozen));
    xrtownershipref root=xrtValueOwnership(handle);xrtownershipsnapshot* proof=NULL;
    assert(xrtOwnershipSnapshotCreate(&root,1,&root,1,admit,NULL,&proof));
    assert(xrtOwnershipSnapshotNodeCount(proof)==2);bool seen=false;
    for(size_t i=0;i<2;++i){xrtownershipnode node;assert(xrtOwnershipSnapshotNode(proof,i,&node)&&!node.Reachable);
        if(node.Reference.Data==native){assert(node.Reference.Ops==xrtNetBytesOwnership(native).Ops);seen=true;}}
    assert(seen);xrtOwnershipSnapshotDestroy(proof);assert(xrtOwnershipScopeEnd(&frozen));xrtValueRelease(handle);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xl6_error_scope initial={0};assert(xl6ErrorScopeBegin(&initial));
    xl6_error_result initialResult=xl6ErrorScopeEnd(&initial);assert(initialResult.ok);xrtErrorFree(initialResult.error);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainNetBytesV1Get(),xl6OwnershipDomainNetBufferNativeV1Get()};
    catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const char data[]={'a',0,'b',(char)255};
    void* raw=xl6NetBytesFromTextV1((xstrview){data,4},20);assert(raw&&xl6NetBytesLengthV1(raw)==4);
    assert(xl6NetBytesByteAtV1(raw,3)==255);
    xvalue* basic=xl6NetBytesToBytesV1(raw,20,1);xbytesview view={0};
    assert(basic&&xrtValueGetBytes(basic,&view)&&view.Size==3&&!memcmp(view.Data,data+1,3));xrtValueRelease(basic);
    xbuffer* output=xrtBufferFrom((xbytesview){(const uint8*)">",1});assert(output);
    assert(xl6NetBytesAppendToV1(raw,output,2,1)==2&&output->Size==3&&!memcmp(output->Data,">\0b",3));
    void* copied=xl6NetBytesFromBufferV1(output,-1);assert(copied);xrtBufferClear(output);
    assert(xl6NetBytesLengthV1(copied)==3&&xl6NetBytesByteAtV1(copied,2)=='b');
    xl6_error_scope error={0};assert(xl6ErrorScopeBegin(&error));assert(!xl6NetBytesToBytesV1(raw,-2,0));errorEnd(&error,XERR_RANGE);
    error=(xl6_error_scope){0};assert(xl6ErrorScopeBegin(&error));(void)xl6NetBytesByteAtV1(raw,4);errorEnd(&error,XERR_RANGE);
    graph(raw);graph(copied);xrtBufferDestroy(output);balanced(&before);
    unsigned failed=0;
    for(unsigned mode=0;mode<2;++mode)for(unsigned budget=0;budget<8;++budget){
        xrtClearError();xrtMemDebugSnapshot(&before);
        void* input=mode?xl6NetBytesCreateV1():NULL;assert(!mode||input);
        error=(xl6_error_scope){0};assert(xl6ErrorScopeBegin(&error)&&xrtMemDebugFailAfter(budget));
        xvalue* handle=NULL;
        if(mode)handle=xl6NativeHandleTakeV1(&input,xl6NetBytesHandlePolicyV1Get());
        else input=xl6NetBytesFromTextV1((xstrview){data,4},-1);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_error_result result=xl6ErrorScopeEnd(&error);
        if(triggered){++failed;assert(mode?handle==NULL:!result.ok);}
        else assert(result.ok&&(mode?handle!=NULL:input!=NULL));
        if(mode)assert(!input);
        xrtErrorFree(result.error);xrtValueRelease(handle);xl6NetBytesDestroyV1(input);balanced(&before);
    }
    assert(failed>0&&failed<16);
    xl6OwnershipCatalogDestroyV1(catalog);assert(xrtMemDebugReset());
    printf("NetBytes: exact two-node owning graph, immutable NUL snapshot, ranges, 16 allocation budgets failures=%u and balanced; no transport\n",failed);
    return 0;
}
