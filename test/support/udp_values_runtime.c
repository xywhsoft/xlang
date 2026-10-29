/* Minimal result-only closure: no Source, Engine, Buffer or Future services. */
#include "include/xrt_decl.h"
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
extern xnetudppacket* udpFixturePacket(const xnetaddr*);
extern xnetudperrorpacket* udpFixtureError(void);
static bool admit(xrtownershipref ref,void* context)
{
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const xrtownershipfinishv1* finish=NULL;
    return xl6OwnershipCatalogProbeV1(ref,context,&adapter,&prepare,&finish)==XL6_OWNERSHIP_ADMISSION_ADMITTED;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));const XL6_OwnershipDomainV1* domains[]={
        #include "udp-language-domains.h"
    };XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(domains[0]));assert(catalog);
    xnetaddr address;assert(xrtNetAddrLoopback(&address,XNET_FAMILY_IPV4,0));
    const XL6_NativeHandlePolicyV1* policies[]={xl6UdpPacketHandlePolicyV1Get(),xl6UdpErrorPacketHandlePolicyV1Get()};
    for(unsigned i=0;i<2;++i){void* raw=i?(void*)udpFixtureError():(void*)udpFixturePacket(&address);assert(raw);
        xvalue* handle=xl6NativeHandleTakeV1(&raw,policies[i]);assert(handle&&!raw);xrtownershipref root=xrtValueOwnership(handle);
        xrtownershipscope frozen={0};assert(xrtOwnershipFreezeTryBegin(&frozen));xrtownershipsnapshot* proof=NULL;
        assert(xrtOwnershipSnapshotCreate(&root,1,&root,1,admit,catalog,&proof)&&xrtOwnershipSnapshotNodeCount(proof)==2);
        for(size_t j=0;j<2;++j){xrtownershipnode node;assert(xrtOwnershipSnapshotNode(proof,j,&node)&&!node.Reachable);}
        xrtOwnershipSnapshotDestroy(proof);assert(xrtOwnershipScopeEnd(&frozen));xrtValueRelease(handle);
    }
    /* A copied Ops table has valid callbacks but is NOT this resident policy. */
    void* raw=udpFixturePacket(&address);assert(raw);xvaluehandleops copied=*policies[0]->Ops;
    xvalue* forged=xrtValueHandleTake(&raw,&copied,NULL);assert(forged&&!raw&&xrtValueHandleOwnershipBindPhased(forged,policies[0]->Trace));
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const xrtownershipfinishv1* finish=NULL;
    assert(xl6OwnershipCatalogProbeV1(xrtValueOwnership(forged),catalog,&adapter,&prepare,&finish)!=XL6_OWNERSHIP_ADMISSION_ADMITTED);
    xrtValueRelease(forged);xl6OwnershipCatalogDestroyV1(catalog);assert(xrtMemDebugReset());
    puts("UDP values only: real native edges, exact Ops identity, no execution services");return 0;
}
