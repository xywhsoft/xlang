#include "include/xruntime_decl.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
extern xnetudppacket* udpFixturePacket(const xnetaddr*);
static XL6_OwnershipCatalog* catalog;
static unsigned prefixes,failures;
static bool admit(xrtownershipref ref,void* unused)
{
    (void)unused;const xrtownershipadapterv1* a=NULL;const xrtownershippreparationv1* p=NULL;const xrtownershipfinishv1* f=NULL;
    return xl6OwnershipCatalogResolveV1(ref,catalog,&a,&p,&f);
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    CHECK(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount);
}
int main(void)
{
    CHECK(xrtMemDebugEnable(true));xl6_error_scope warm={0};CHECK(xl6ErrorScopeBegin(&warm));
    xl6_error_result done=xl6ErrorScopeEnd(&warm);CHECK(done.ok);xrtErrorFree(done.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "udp-language-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));CHECK(catalog);
    xnetaddr address;CHECK(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));
    xnetudppacket* packet=udpFixturePacket(&address);CHECK(packet);
    xnetbytes* bytes=xrtNetBytesCopy("b\0x",3);CHECK(bytes);
    void* base=xl6UdpMessageTextV1((xstrview){"a\0b",3},-1);CHECK(base);
    xnetdgramcontrol control={0};
    for(unsigned kind=0;kind<5;++kind){
        for(unsigned budget=0;;++budget){
            CHECK(budget<128);xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            xl6_error_scope scope={0};CHECK(xl6ErrorScopeBegin(&scope)&&xrtMemDebugFailAfter(budget));
            void* raw=kind==0?xl6UdpMessageTextV1((xstrview){"a\0b",3},99):
                kind==1?xl6UdpMessageRefBytesV1(bytes,99,1):kind==2?xl6UdpMessageRefPacketV1(packet,3,1):
                kind==3?xl6UdpMessageControlV1(base,&control):xl6UdpMessageTargetV1(base,&address);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();done=xl6ErrorScopeEnd(&scope);
            CHECK((raw!=NULL)==done.ok&&(!raw)==hit);xrtErrorFree(done.error);
            if(raw){
                CHECK(xl6UdpMessageLengthV1(raw)==(kind==1?2:3));
                xvalue* root=xl6NativeHandleTakeV1(&raw,xl6UdpMessageHandlePolicyV1Get());CHECK(root&&!raw);
                xrtownershipref ref=xrtValueOwnership(root);xrtownershipscope freeze={0};CHECK(xrtOwnershipFreezeTryBegin(&freeze));
                xrtownershipsnapshot* graph=NULL;CHECK(xrtOwnershipSnapshotCreate(&ref,1,&ref,1,admit,NULL,&graph));
                CHECK(xrtOwnershipSnapshotNodeCount(graph)>=2);xrtOwnershipSnapshotDestroy(graph);CHECK(xrtOwnershipScopeEnd(&freeze));
                xrtValueRelease(root);
            }else ++failures;
            balanced(&before);++prefixes;if(!hit)break;
        }
    }
    xl6UdpMessageDestroyV1(base);xrtNetBytesDestroy(bytes);xrtNetUdpPacketDestroy(packet);
    xl6OwnershipCatalogDestroyV1(catalog);CHECK(xrtMemDebugReset());
    printf("UDP immutable message: full prefixes=%u failures=%u; actual native refs, descriptor clones, graph policies; balanced\n",prefixes,failures);return 0;
}
