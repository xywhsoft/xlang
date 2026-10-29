#include "xl6_x509_crl_critical_internal.h"

void* xl6X509CrlCriticalCreateV1(void)
{
    XL6_X509CrlCriticalV1* self=xrtCalloc(1,sizeof(*self));if(!self)return NULL;
    xrtAtomic64Init(&self->references,1);xrtAtomic64Init(&self->owner,0);return self;
}
void xl6X509CrlCriticalDestroyV1(void* raw)
{
    XL6_X509CrlCriticalV1* self=raw;
    if(self && xrtAtomic64FetchSub(&self->references,1,XMEMORY_ACQ_REL)==1)xl6X509FreeV1(self);
}
static const XL6_X509CrlCriticalV1* current(void* raw)
{
    const XL6_X509CrlCriticalV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing CRL critical context");return NULL;}
    uint64 owner=xrtAtomic64Load(&self->owner,XMEMORY_ACQUIRE);
    /* No non-owner may read the non-atomic fields. A live owner thread cannot
     * have its ID recycled; inactive contexts have no borrowed view to read. */
    if(!owner || owner!=xrtThreadCurrentId() || !self->active) {
        xl6X509FailV1(XERR_STATE,"CRL critical context is inactive or belongs to another thread");return NULL;
    }
    return self;
}
void* xl6X509CrlCriticalEntryV1(void* raw)
{
    const XL6_X509CrlCriticalV1* self=current(raw);
    return self && self->entry?xl6X509CrlEntryCopyV1(self->entry->Raw,self->version):NULL;
}
void* xl6X509CrlCriticalExtensionV1(void* raw)
{
    const XL6_X509CrlCriticalV1* self=current(raw);
    return self?xl6X509ExtensionCopyV1(self->extension):NULL;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509CrlCriticalHandlePolicyV1Get,xl6X509CrlCriticalDestroyV1)
