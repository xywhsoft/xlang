/* Private owned-packet preparation only. No socket, TLS handshake, trust
 * decision, malformed certificate or crypto/security campaign is executed. */
#include "src/runtime/xl6_tls_verified_peer_packet.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "test/support/tls_verified_peer_packet_data.h"

typedef struct Fixture {
    uint8 leaf[sizeof(verifiedPeerLeafDer)];
    uint8 mid[sizeof(verifiedPeerMidDer)];
    uint8 root[sizeof(verifiedPeerRootDer)];
    char name[9];
    xx509cert certificates[2];
    xx509cert anchorCertificate;
    xx509anchor anchor;
    const xx509cert* path[2];
    xtlspeer peer;
    xtlsverifiedpeer verified;
} Fixture;

static void initialize(Fixture* f)
{
    memset(f,0,sizeof(*f));
    memcpy(f->leaf,verifiedPeerLeafDer,sizeof(f->leaf));
    memcpy(f->mid,verifiedPeerMidDer,sizeof(f->mid));
    memcpy(f->root,verifiedPeerRootDer,sizeof(f->root));
    memcpy(f->name,"localhost",sizeof(f->name));
    assert(xrtX509Parse(f->leaf,sizeof(f->leaf),&f->certificates[0]));
    assert(xrtX509Parse(f->mid,sizeof(f->mid),&f->certificates[1]));
    assert(xrtX509Parse(f->root,sizeof(f->root),&f->anchorCertificate));
    assert(xrtX509Anchor(&f->anchorCertificate,&f->anchor));
    f->path[0]=&f->certificates[0];f->path[1]=&f->certificates[1];
    f->peer=(xtlspeer){XTLS_SERVER,{f->name,sizeof(f->name)},INT64_C(1893456000000000),f->certificates,2};
    f->verified=(xtlsverifiedpeer){&f->peer,f->path,2,&f->anchor};
}

static void equal(xbytesview span,const uint8* bytes,size_t size)
{assert(span.Size==size && span.Data && !memcmp(span.Data,bytes,size));}

static void observations(const XL6_TlsVerifiedPeerPacketV1* packet)
{
    assert(xl6TlsVerifiedPeerPacketRoleV1(packet)==XTLS_SERVER);
    assert(xl6TlsVerifiedPeerPacketTimeV1(packet)==INT64_C(1893456000000000));
    xstrview name=xl6TlsVerifiedPeerPacketNameViewV1(packet);
    assert(name.Size==9 && !memcmp(name.Data,"localhost",9));
    assert(xl6TlsVerifiedPeerPacketCertificateCountV1(packet)==2);
    assert(xl6TlsVerifiedPeerPacketPathCountV1(packet)==2);
    xbytesview leaf=xl6TlsVerifiedPeerPacketCertificateDerV1(packet,0);
    xbytesview mid=xl6TlsVerifiedPeerPacketCertificateDerV1(packet,1);
    equal(leaf,verifiedPeerLeafDer,sizeof(verifiedPeerLeafDer));
    equal(mid,verifiedPeerMidDer,sizeof(verifiedPeerMidDer));
    equal(xl6TlsVerifiedPeerPacketAnchorDerV1(packet),verifiedPeerRootDer,sizeof(verifiedPeerRootDer));
    /* Native identity aliases share one private DER copy, not source memory. */
    assert(xl6TlsVerifiedPeerPacketPathDerV1(packet,0).Data==leaf.Data);
    assert(xl6TlsVerifiedPeerPacketPathDerV1(packet,1).Data==mid.Data);
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount);
    assert(before->DoubleFreeCount==after.DoubleFreeCount);
    assert(before->UseAfterFreeCount==after.UseAfterFreeCount);
}

static void detached(void)
{
    Fixture f;initialize(&f);
    xmemdebugsnapshot before,created;xrtMemDebugSnapshot(&before);
    XL6_TlsVerifiedPeerPacketV1* packet=xl6TlsVerifiedPeerPacketCopyV1(&f.verified);assert(packet);
    xrtMemDebugSnapshot(&created);assert(created.AllocCount-before.AllocCount==1);
    memset(&f,0,sizeof(f));observations(packet);
    XL6_TlsVerifiedPeerPacketV1* retained=xl6TlsVerifiedPeerPacketRefV1(packet);assert(retained==packet);
    xl6TlsVerifiedPeerPacketDropV1(packet);observations(retained);
    xl6TlsVerifiedPeerPacketDropV1(retained);balanced(&before);
}

static void allocationPrefix(size_t point)
{
    Fixture f;initialize(&f);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(point));
    XL6_TlsVerifiedPeerPacketV1* packet=xl6TlsVerifiedPeerPacketCopyV1(&f.verified);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(hit)assert(!packet && xrtGetError());
    else {assert(packet);observations(packet);}
    if(!point)assert(hit);
    xrtClearError();xl6TlsVerifiedPeerPacketDropV1(packet);balanced(&before);
}

static void distinctViewsAndLargeChain(void)
{
    Fixture f;initialize(&f);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    /* Equal raw bytes in a separately located native view are not assumed
     * to belong to the presented array. Both views remain independently owned. */
    xx509cert other=f.certificates[0];f.path[0]=&other;
    XL6_TlsVerifiedPeerPacketV1* packet=xl6TlsVerifiedPeerPacketCopyV1(&f.verified);assert(packet);
    assert(xl6TlsVerifiedPeerPacketPathDerV1(packet,0).Data!=
           xl6TlsVerifiedPeerPacketCertificateDerV1(packet,0).Data);
    equal(xl6TlsVerifiedPeerPacketPathDerV1(packet,0),verifiedPeerLeafDer,sizeof(verifiedPeerLeafDer));
    xl6TlsVerifiedPeerPacketDropV1(packet);
    /* This is a data-copy boundary, not a fabricated TLS handshake: retain
     * all 512 valid parsed views without a hidden small-chain truncation. */
    size_t count=512;
    xx509cert* views=xrtCalloc(count,sizeof(*views));assert(views);
    for(size_t i=0;i<count;++i)views[i]=f.certificates[0];
    f.peer.Certificates=views;f.peer.CertificateCount=count;
    f.path[0]=&views[0];f.path[1]=&views[count-1];
    packet=xl6TlsVerifiedPeerPacketCopyV1(&f.verified);assert(packet);
    xrtFree(views);
    assert(xl6TlsVerifiedPeerPacketCertificateCountV1(packet)==count);
    equal(xl6TlsVerifiedPeerPacketCertificateDerV1(packet,count-1),verifiedPeerLeafDer,sizeof(verifiedPeerLeafDer));
    assert(xl6TlsVerifiedPeerPacketPathDerV1(packet,1).Data==
           xl6TlsVerifiedPeerPacketCertificateDerV1(packet,count-1).Data);
    xl6TlsVerifiedPeerPacketDropV1(packet);balanced(&before);
}

static void boundaries(void)
{
    Fixture f;initialize(&f);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    assert(!xl6TlsVerifiedPeerPacketCopyV1(NULL) && xrtGetError());xrtClearError();
    f.peer.CertificateCount=SIZE_MAX;
    assert(!xl6TlsVerifiedPeerPacketCopyV1(&f.verified) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    f.peer.CertificateCount=2;f.verified.PathCount=SIZE_MAX;
    assert(!xl6TlsVerifiedPeerPacketCopyV1(&f.verified) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    f.verified.PathCount=2;f.peer.Name.Size=SIZE_MAX;
    assert(!xl6TlsVerifiedPeerPacketCopyV1(&f.verified) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    f.peer.Name.Size=9;f.anchor.Certificate.Size=SIZE_MAX;
    assert(!xl6TlsVerifiedPeerPacketCopyV1(&f.verified) && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    f.anchor.Certificate=f.anchorCertificate.Raw;
    XL6_TlsVerifiedPeerPacketV1* packet=xl6TlsVerifiedPeerPacketCopyV1(&f.verified);assert(packet);
    assert(!xl6TlsVerifiedPeerPacketCertificateDerV1(packet,2).Data && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    assert(!xl6TlsVerifiedPeerPacketPathDerV1(packet,SIZE_MAX).Data && xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    xrtSetErrorKind(XERR_STATE);const xerror* prior=xrtGetError();
    assert(!xl6TlsVerifiedPeerPacketCopyV1(&f.verified) && xrtGetError()==prior);
    assert(!xl6TlsVerifiedPeerPacketRefV1(packet) && xrtGetError()==prior);
    xl6TlsVerifiedPeerPacketDropV1(packet);assert(xrtGetError()==prior);xrtClearError();
    f.verified.PathCount=0;f.verified.Path=NULL;f.peer.Name=(xstrview){0};
    f.peer.Role=XTLS_CLIENT;f.peer.Time=0;
    packet=xl6TlsVerifiedPeerPacketCopyV1(&f.verified);assert(packet);
    assert(xl6TlsVerifiedPeerPacketRoleV1(packet)==XTLS_CLIENT);
    assert(xl6TlsVerifiedPeerPacketTimeV1(packet)==0 && !xrtGetError());
    assert(xl6TlsVerifiedPeerPacketPathCountV1(packet)==0);
    assert(xl6TlsVerifiedPeerPacketNameViewV1(packet).Size==0);
    equal(xl6TlsVerifiedPeerPacketAnchorDerV1(packet),verifiedPeerRootDer,sizeof(verifiedPeerRootDer));
    xl6TlsVerifiedPeerPacketDropV1(packet);balanced(&before);
}

static int32 reader(void* raw)
{
    XL6_TlsVerifiedPeerPacketV1* packet=raw;
    for(unsigned i=0;i<2000;++i) {
        XL6_TlsVerifiedPeerPacketV1* copy=xl6TlsVerifiedPeerPacketRefV1(packet);
        assert(copy);observations(copy);xl6TlsVerifiedPeerPacketDropV1(copy);
    }
    return 0;
}

static void sharedReads(void)
{
    Fixture f;initialize(&f);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_TlsVerifiedPeerPacketV1* packet=xl6TlsVerifiedPeerPacketCopyV1(&f.verified);assert(packet);
    xthread* a=xrtThreadCreate(reader,packet,0);assert(a);
    xthread* b=xrtThreadCreate(reader,packet,0);assert(b);
    assert(xrtThreadWait(a)==XWAIT_OK && xrtThreadWait(b)==XWAIT_OK);
    assert(xrtThreadExitCode(a)==0 && xrtThreadExitCode(b)==0);
    xrtThreadDestroy(a);xrtThreadDestroy(b);
    xl6TlsVerifiedPeerPacketDropV1(packet);balanced(&before);
}

int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    detached();boundaries();distinctViewsAndLargeChain();sharedReads();
    for(size_t point=0;point<32;++point)allocationPrefix(point);
    puts("verified-peer packet: one allocation, detached DER/name, aliases, 4000 shared reads and 32 allocation prefixes balanced: ok");
    return 0;
}
