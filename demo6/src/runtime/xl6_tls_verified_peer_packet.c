#include "xl6_tls_verified_peer_packet.h"
#include "xl6_tls_internal.h"
#include <limits.h>
#include <string.h>

struct XL6_TlsVerifiedPeerPacketV1 {
    xatomic64 references;
    int64_t role;
    int64_t at;
    size_t certificateCount;
    size_t pathCount;
    xstrview name;
    /* Presented certificates, selected path, then the separate anchor. */
    xbytesview certificates[];
};

static xbytesview sourceDer(const xtlsverifiedpeer* source,size_t index)
{
    if(index<source->Peer->CertificateCount)
        return source->Peer->Certificates[index].Raw;
    index-=source->Peer->CertificateCount;
    if(index<source->PathCount) {
        if(!source->Path[index]) {
            xl6TlsFailV1(XERR_ARGUMENT,"missing verified TLS path certificate");
            return (xbytesview){0};
        }
        return source->Path[index]->Raw;
    }
    return source->Anchor->Certificate;
}

static bool presentedAlias(const xtlsverifiedpeer* source,size_t index,size_t* earlier)
{
    size_t count=source->Peer->CertificateCount;
    if(index<count || index-count>=source->PathCount)return false;
    /* The native selected path normally points directly into the presented
     * certificate array. Recognize that identity in constant time without
     * ordering or subtracting pointers to unrelated C objects. Other native
     * views (e.g. issuers supplied by the trust store) get their own copy. */
    uintptr_t address=(uintptr_t)source->Path[index-count];
    uintptr_t first=(uintptr_t)source->Peer->Certificates;
    if(address<first)return false;
    uintptr_t offset=address-first;
    if(offset%sizeof(xx509cert) || offset/sizeof(xx509cert)>=count)return false;
    *earlier=(size_t)(offset/sizeof(xx509cert));
    return true;
}

XL6_TlsVerifiedPeerPacketV1* xl6TlsVerifiedPeerPacketCopyV1(const xtlsverifiedpeer* source)
{
    if(xrtGetError())return NULL;
    if(!source || !source->Peer || !source->Anchor ||
       !source->Peer->CertificateCount || !source->Peer->Certificates ||
       (source->PathCount && !source->Path) ||
       (source->Peer->Name.Size && !source->Peer->Name.Data) ||
       (source->Peer->Role!=XTLS_CLIENT && source->Peer->Role!=XTLS_SERVER)) {
        xl6TlsFailV1(XERR_ARGUMENT,"incomplete verified TLS peer snapshot source");
        return NULL;
    }
    size_t peers=source->Peer->CertificateCount,paths=source->PathCount;
    if(peers>INT64_MAX || paths>INT64_MAX || peers>SIZE_MAX-paths ||
       peers+paths==SIZE_MAX) {
        xl6TlsFailV1(XERR_RANGE,"verified TLS peer certificate count overflows");
        return NULL;
    }
    size_t count=peers+paths+1;
    if(count>(SIZE_MAX-sizeof(XL6_TlsVerifiedPeerPacketV1))/sizeof(xbytesview)) {
        xl6TlsFailV1(XERR_RANGE,"verified TLS peer view table overflows");
        return NULL;
    }
    size_t total=sizeof(XL6_TlsVerifiedPeerPacketV1)+count*sizeof(xbytesview);
    if(source->Peer->Name.Size>INT64_MAX || source->Peer->Name.Size>SIZE_MAX-total) {
        xl6TlsFailV1(XERR_RANGE,"verified TLS reference name exceeds its snapshot range");
        return NULL;
    }
    total+=source->Peer->Name.Size;
    for(size_t i=0;i<count;++i) {
        xbytesview span=sourceDer(source,i);
        if(xrtGetError())return NULL;
        if(!span.Data || !span.Size) {
            xl6TlsFailV1(XERR_ARGUMENT,"verified TLS snapshot requires complete certificate DER");
            return NULL;
        }
        if(span.Size>INT64_MAX) {
            xl6TlsFailV1(XERR_RANGE,"verified TLS certificate exceeds its language range");
            return NULL;
        }
        size_t earlier;
        if(presentedAlias(source,i,&earlier))continue;
        if(span.Size>SIZE_MAX-total) {
            xl6TlsFailV1(XERR_RANGE,"verified TLS certificate bytes overflow");
            return NULL;
        }
        total+=span.Size;
    }
    XL6_TlsVerifiedPeerPacketV1* self=xrtMalloc(total);
    if(!self)return NULL;
    xrtAtomic64Store(&self->references,1,XMEMORY_RELAXED);
    self->role=(int64_t)source->Peer->Role;
    self->at=source->Peer->Time;
    self->certificateCount=peers;
    self->pathCount=paths;
    uint8* cursor=(uint8*)(self->certificates+count);
    self->name=(xstrview){source->Peer->Name.Size?(const char*)cursor:NULL,source->Peer->Name.Size};
    if(self->name.Size)memcpy(cursor,source->Peer->Name.Data,self->name.Size);
    cursor+=self->name.Size;
    for(size_t i=0;i<count;++i) {
        xbytesview span=sourceDer(source,i);
        size_t earlier;
        /* Identity aliases share a private copy without a quadratic scan,
         * content comparison, hash table, or additional allocation. */
        if(presentedAlias(source,i,&earlier))self->certificates[i]=self->certificates[earlier];
        else {
            self->certificates[i]=(xbytesview){cursor,span.Size};
            memcpy(cursor,span.Data,span.Size);
            cursor+=span.Size;
        }
    }
    return self;
}

XL6_TlsVerifiedPeerPacketV1* xl6TlsVerifiedPeerPacketRefV1(XL6_TlsVerifiedPeerPacketV1* self)
{
    if(xrtGetError())return NULL;
    if(!self) {
        xl6TlsFailV1(XERR_ARGUMENT,"missing verified TLS peer snapshot");
        return NULL;
    }
    uint64 count=xrtAtomic64Load(&self->references,XMEMORY_RELAXED);
    for(;;) {
        if(!count || count==UINT64_MAX) {
            xl6TlsFailV1(XERR_RANGE,"verified TLS peer snapshot owner count is exhausted");
            return NULL;
        }
        if(xrtAtomic64CompareExchange(&self->references,&count,count+1,
                                     XMEMORY_RELAXED,XMEMORY_RELAXED))return self;
    }
}

void xl6TlsVerifiedPeerPacketDropV1(XL6_TlsVerifiedPeerPacketV1* self)
{
    if(!self || xrtAtomic64FetchSub(&self->references,1,XMEMORY_ACQ_REL)!=1)return;
    xerror* prior=xrtTakeError();
    xrtFree(self);
    if(prior) {
        xrtClearError();
        xrtSetErrorTake(prior);
    }
}

static const XL6_TlsVerifiedPeerPacketV1* checked(const XL6_TlsVerifiedPeerPacketV1* self)
{
    if(xrtGetError())return NULL;
    if(!self)xl6TlsFailV1(XERR_ARGUMENT,"missing verified TLS peer snapshot");
    return self;
}

int64_t xl6TlsVerifiedPeerPacketRoleV1(const XL6_TlsVerifiedPeerPacketV1* raw)
{const XL6_TlsVerifiedPeerPacketV1* self=checked(raw);return self?self->role:0;}
int64_t xl6TlsVerifiedPeerPacketTimeV1(const XL6_TlsVerifiedPeerPacketV1* raw)
{const XL6_TlsVerifiedPeerPacketV1* self=checked(raw);return self?self->at:0;}
xstrview xl6TlsVerifiedPeerPacketNameViewV1(const XL6_TlsVerifiedPeerPacketV1* raw)
{const XL6_TlsVerifiedPeerPacketV1* self=checked(raw);return self?self->name:(xstrview){0};}
size_t xl6TlsVerifiedPeerPacketCertificateCountV1(const XL6_TlsVerifiedPeerPacketV1* raw)
{const XL6_TlsVerifiedPeerPacketV1* self=checked(raw);return self?self->certificateCount:0;}
size_t xl6TlsVerifiedPeerPacketPathCountV1(const XL6_TlsVerifiedPeerPacketV1* raw)
{const XL6_TlsVerifiedPeerPacketV1* self=checked(raw);return self?self->pathCount:0;}

xbytesview xl6TlsVerifiedPeerPacketCertificateDerV1(const XL6_TlsVerifiedPeerPacketV1* raw,size_t index)
{
    const XL6_TlsVerifiedPeerPacketV1* self=checked(raw);if(!self)return (xbytesview){0};
    if(index>=self->certificateCount) {
        xl6TlsFailV1(XERR_RANGE,"verified TLS peer certificate index is out of range");
        return (xbytesview){0};
    }
    return self->certificates[index];
}
xbytesview xl6TlsVerifiedPeerPacketPathDerV1(const XL6_TlsVerifiedPeerPacketV1* raw,size_t index)
{
    const XL6_TlsVerifiedPeerPacketV1* self=checked(raw);if(!self)return (xbytesview){0};
    if(index>=self->pathCount) {
        xl6TlsFailV1(XERR_RANGE,"verified TLS path certificate index is out of range");
        return (xbytesview){0};
    }
    return self->certificates[self->certificateCount+index];
}
xbytesview xl6TlsVerifiedPeerPacketAnchorDerV1(const XL6_TlsVerifiedPeerPacketV1* raw)
{
    const XL6_TlsVerifiedPeerPacketV1* self=checked(raw);
    return self?self->certificates[self->certificateCount+self->pathCount]:(xbytesview){0};
}
