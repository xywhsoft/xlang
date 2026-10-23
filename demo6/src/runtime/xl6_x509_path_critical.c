#include "xl6_x509_path_critical_internal.h"
void* xl6X509PathCriticalCreateV1(void)
{
    XL6_X509PathCriticalV1* self=xrtCalloc(1,sizeof(*self));
    if(self){xrtAtomic64Init(&self->references,1);xrtAtomic64Init(&self->owner,0);}return self;
}
void xl6X509PathCriticalDestroyV1(void* raw)
{
    XL6_X509PathCriticalV1* self=raw;
    if(self && xrtAtomic64FetchSub(&self->references,1,XMEMORY_ACQ_REL)==1)xl6X509FreeV1(self);
}
static const XL6_X509PathCriticalV1* current(void* raw)
{
    const XL6_X509PathCriticalV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing path callback context");return NULL;}
    uint64 owner=xrtAtomic64Load(&self->owner,XMEMORY_ACQUIRE);
    if(!owner || owner!=xrtThreadCurrentId() || !self->active) {
        xl6X509FailV1(XERR_STATE,"path callback context is inactive or belongs to another thread");return NULL;
    }
    return self;
}
void* xl6X509PathCriticalCertificateV1(void* raw)
{const XL6_X509PathCriticalV1* p=current(raw);return p?xl6X509CertificateCreateSpanV1(p->certificate->Raw):NULL;}
void* xl6X509PathCriticalExtensionV1(void* raw)
{const XL6_X509PathCriticalV1* p=current(raw);return p?xl6X509ExtensionCopyV1(p->extension):NULL;}
int64_t xl6X509PathCriticalDepthV1(void* raw)
{const XL6_X509PathCriticalV1* p=current(raw);return p?(int64_t)p->depth:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509PathCriticalHandlePolicyV1Get,xl6X509PathCriticalDestroyV1)
