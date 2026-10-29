#include "xl6_x509_path_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509PathDestroyV1(void* raw)
{
    XL6_X509PathV1* self=raw;
    if(self && xrtAtomic64FetchSub(&self->references,1,XMEMORY_ACQ_REL)==1)xl6X509FreeV1(self);
}
void* xl6X509PathRefV1(void* raw)
{
    XL6_X509PathV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing validated path");return NULL;}
    return xl6X509PolicyRetainV1(&self->references)?self:NULL;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509PathHandlePolicyV1Get,xl6X509PathDestroyV1)
