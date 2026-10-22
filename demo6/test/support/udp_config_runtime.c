/* Offline POD configuration/graph/OOM only. No Engine/socket or I/O. */
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
static unsigned failures,budgets,builders;
static XL6_OwnershipCatalog* catalog;
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount&&before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
static void getters(const void* raw)
{
    const xnetudpconfig* c=raw;
    #define XL6_UDP_CONFIG_SIZE(Field,Name) CHECK(xl6UdpConfig##Field##V1(raw)==c->Field);
    #define XL6_UDP_CONFIG_UINT(Field,Name) XL6_UDP_CONFIG_SIZE(Field,Name)
    #define XL6_UDP_CONFIG_INT(Field,Name) XL6_UDP_CONFIG_SIZE(Field,Name)
    #define XL6_UDP_CONFIG_BOOL(Field,Name) XL6_UDP_CONFIG_SIZE(Field,Name)
    #include "src/runtime/xl6_udp_config_fields.h"
    #undef XL6_UDP_CONFIG_SIZE
    #undef XL6_UDP_CONFIG_UINT
    #undef XL6_UDP_CONFIG_INT
    #undef XL6_UDP_CONFIG_BOOL
    CHECK(xl6UdpConfigReceiveMetadataV1(raw)==c->ReceiveMeta&&xl6UdpConfigOverflowV1(raw)==c->Overflow&&
        xl6UdpConfigTruncationV1(raw)==c->Truncation&&xl6UdpConfigErrorOverflowV1(raw)==c->ErrorOverflow&&xl6UdpConfigPathMtuV1(raw)==c->PathMtu);
}
static void* build(unsigned i,const void* c)
{
    switch(i){
        case 0:return xl6UdpConfigWithReceiveV1(c,4096,2,32);
        case 1:return xl6UdpConfigWithReceiveQueueV1(c,128,65536,XNET_UDP_DROP_OLDEST);
        case 2:return xl6UdpConfigWithReceiveMetadataV1(c,XNET_DGRAM_META_DESTINATION|XNET_DGRAM_META_INTERFACE);
        case 3:return xl6UdpConfigWithTruncationV1(c,XNET_UDP_TRUNCATE_DROP);
        case 4:return xl6UdpConfigWithSendQueueV1(c,4096,1024,8192,32);
        case 5:return xl6UdpConfigWithSendConcurrencyV1(c,2);
        case 6:return xl6UdpConfigWithSocketBuffersV1(c,65536,8192);
        case 7:return xl6UdpConfigWithTrafficV1(c,64,32);
        case 8:return xl6UdpConfigWithPathMtuV1(c,XNET_PMTU_DISCOVER);
        case 9:return xl6UdpConfigWithBindingV1(c,false,false,false,true);
        case 10:return xl6UdpConfigWithBroadcastV1(c,true);
        case 11:return xl6UdpConfigWithErrorQueueV1(c,true,512,16,32768,XNET_UDP_DROP_OLDEST);
        default:abort();
    }
}
static void error(void* result,xl6_error_scope* scope)
{
    xl6_error_result outcome=xl6ErrorScopeEnd(scope);CHECK(!result&&!outcome.ok);
    CHECK(outcome.error);xerrkind kind=xrtErrorKind(outcome.error);CHECK(kind==XERR_RANGE||kind==XERR_ARGUMENT);
    xrtErrorFree(outcome.error);++failures;
}
static bool admit(xrtownershipref ref,void* context)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const xrtownershipfinishv1* finish=NULL;
    return xl6OwnershipCatalogProbeV1(ref,context,&adapter,&prepare,&finish)==XL6_OWNERSHIP_ADMISSION_ADMITTED;
}
static void graph(void* raw)
{
    const XL6_NativeHandlePolicyV1* policy=xl6UdpConfigHandlePolicyV1Get();xvalue* handle=xl6NativeHandleTakeV1(&raw,policy);CHECK(handle&&!raw);
    xrtownershipref ref=xrtValueOwnership(handle);xrtownershipscope scope={0};CHECK(xrtOwnershipFreezeTryBegin(&scope));
    xrtownershipsnapshot* proof=NULL;CHECK(xrtOwnershipSnapshotCreate(&ref,1,&ref,1,admit,catalog,&proof));
    CHECK(xrtOwnershipSnapshotNodeCount(proof)==1);xrtownershipnode node;CHECK(xrtOwnershipSnapshotNode(proof,0,&node)&&!node.Reachable);
    xrtOwnershipSnapshotDestroy(proof);CHECK(xrtOwnershipScopeEnd(&scope));xrtValueRelease(handle);
}
int main(void)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope seed={0};CHECK(xl6ErrorScopeBegin(&seed));
    xl6_error_result first=xl6ErrorScopeEnd(&seed);CHECK(first.ok);xrtErrorFree(first.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "udp-config-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));CHECK(catalog);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);void* base=xl6UdpConfigCreateV1();CHECK(base);getters(base);
    xnetudpconfig original=*(const xnetudpconfig*)base;
    for(unsigned i=0;i<12;++i){void* changed=build(i,base);CHECK(changed&&changed!=base&&memcmp(base,&original,sizeof(original))==0);
        getters(changed);graph(changed);++builders;}
    #define BAD(Call) do{xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope));error((Call),&scope);CHECK(!memcmp(base,&original,sizeof(original)));}while(0)
    BAD(xl6UdpConfigWithReceiveV1(base,0,1,1));BAD(xl6UdpConfigWithReceiveV1(base,1,UINT64_MAX,1));
    BAD(xl6UdpConfigWithReceiveV1(base,1,1,1025));BAD(xl6UdpConfigWithReceiveQueueV1(base,1,1,INT64_MAX));
    BAD(xl6UdpConfigWithReceiveMetadataV1(base,INT64_MAX));BAD(xl6UdpConfigWithReceiveMetadataV1(base,XNET_DGRAM_META_TRUNCATED));
    BAD(xl6UdpConfigWithTruncationV1(base,3));BAD(xl6UdpConfigWithSendQueueV1(base,100,101,1000,1));
    BAD(xl6UdpConfigWithSendConcurrencyV1(base,65));BAD(xl6UdpConfigWithSendConcurrencyV1(base,UINT64_MAX));
    BAD(xl6UdpConfigWithSocketBuffersV1(base,INT64_MAX,1));BAD(xl6UdpConfigWithSocketBuffersV1(base,-2,1));
    BAD(xl6UdpConfigWithTrafficV1(base,INT64_MAX,1));BAD(xl6UdpConfigWithTrafficV1(base,256,1));
    BAD(xl6UdpConfigWithPathMtuV1(base,4));BAD(xl6UdpConfigWithBindingV1(base,true,false,true,false));
    BAD(xl6UdpConfigWithErrorQueueV1(base,true,65536,1,1,0));BAD(xl6UdpConfigWithErrorQueueV1(base,true,1,1,1,-1));
    for(unsigned i=0;i<12;++i)for(unsigned budget=0;budget<3;++budget){
        xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(budget));void* value=build(i,base);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        CHECK((value!=NULL)==outcome.ok&&(value!=NULL)==!triggered);xrtErrorFree(outcome.error);xl6UdpConfigDestroyV1(value);
        CHECK(!memcmp(base,&original,sizeof(original)));++budgets;
    }
    xl6UdpConfigDestroyV1(base);balanced(&before);xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("UDP configuration: builders=%u invalid=%u OOM-budgets=%u; immutable leaf graph, all fields, no network\n",builders,failures,budgets);return 0;
}
