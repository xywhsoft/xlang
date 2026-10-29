/* Compile the exact standalone Pool/Bytes catalog closure, without Engine. */
#include "net-buffer-native-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    xl6_error_result error=xl6ErrorScopeEnd(&scope);assert(error.ok);xrtErrorFree(error.error);
    const XL6_OwnershipDomainV1* domains[1]={xl6OwnershipDomainNetBufferNativeV1Get()};
    unsigned failures=0;
    for(unsigned budget=0;budget<5;++budget){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(budget));
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        assert((catalog!=NULL)!=failed);if(failed)++failures;xrtClearError();
        xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
    }
    assert(failures>0&&failures<5);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,1);assert(catalog);
    xnetbufpool* pool=xrtNetBufPoolCreate(NULL);xnetbytes* bytes=xrtNetBytesCopy("a\0b",3);assert(pool&&bytes);
    xrtownershipscope frozen={0};assert(xrtOwnershipFreezeTryBegin(&frozen));
    const xrtownershipadapterv1 *pa=NULL,*ba=NULL;const xrtownershippreparationv1 *pp=NULL,*bp=NULL;
    const XL6_OwnershipFinishV1 *pf=NULL,*bf=NULL;size_t count=0;int token=0;
    xrtownershipref pr=xrtNetBufPoolOwnership(pool),br=xrtNetBytesOwnership(bytes);
    assert(xl6OwnershipCatalogProbeV1(pr,catalog,&pa,&pp,&pf)==XL6_OWNERSHIP_ADMISSION_ADMITTED&&pa&&pp&&!pf);
    assert(xl6OwnershipCatalogProbeV1(br,catalog,&ba,&bp,&bf)==XL6_OWNERSHIP_ADMISSION_ADMITTED&&ba&&!bp&&!bf);
    assert(pr.Ops->Count(pool,&count)&&count==1&&br.Ops->Count(bytes,&count)&&count==1);
    assert(pa->Hold(pool)&&ba->Hold(bytes)&&pa->Claim(pool,&token)&&ba->Claim(bytes,&token));
    assert(xrtOwnershipScopeEnd(&frozen));assert(pp->Prepare(pool,&token)==XRT_OWNERSHIP_PREPARE_READY);
    assert(xrtOwnershipFreezeTryBegin(&frozen)&&pp->Ready(pool));pa->Clear(pool,&token);ba->Clear(bytes,&token);
    assert(xrtOwnershipScopeEnd(&frozen)&&xrtNetBufPoolDestroy(pool));xrtNetBytesDestroy(bytes);
    assert(xrtNetBytesView(bytes).Size==3&&memcmp(xrtNetBytesView(bytes).Data,"a\0b",3)==0);
    pa->Drop(pool);ba->Drop(bytes);xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
    assert(xrtMemDebugReset());
    printf("minimal native buffer catalog: actual Pool/Bytes, five catalog OOM prefixes, failures=%u; balanced\n",failures);return 0;
}
