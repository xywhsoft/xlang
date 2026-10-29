#include "xl6_x509_store_internal.h"
#include "src/api/xl_foreign_error.h"

XL6_X509StoreVersionV1* xl6X509StoreVersionTakeV1(xx509store** slot,int64_t bytes)
{
    if(!slot || !*slot || bytes<0){xl6X509FailV1(XERR_ARGUMENT,"invalid trust store version adoption");return NULL;}
    xx509store* value=*slot;*slot=NULL;
    XL6_X509StoreVersionV1* self=xrtCalloc(1,sizeof(*self));
    if(!self){xerror* error=xrtTakeError();xrtX509StoreFree(value);xrtSetErrorTake(error);return NULL;}
    xrtAtomic64Init(&self->references,1);self->value=value;self->bytes=bytes;return self;
}
XL6_X509StoreVersionV1* xl6X509StoreVersionRefV1(XL6_X509StoreVersionV1* self)
{
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing trust store snapshot");return NULL;}
    return xl6X509PolicyRetainV1(&self->references)?self:NULL;
}
void xl6X509StoreSnapshotDestroyV1(void* raw)
{
    XL6_X509StoreVersionV1* self=raw;
    if(self && xrtAtomic64FetchSub(&self->references,1,XMEMORY_ACQ_REL)==1){
        xerror* error=xrtTakeError();xrtX509StoreFree(self->value);xl6X509FreeV1(self);
        if(error){xrtClearError();xrtSetErrorTake(error);}
    }
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509StoreSnapshotHandlePolicyV1Get,xl6X509StoreSnapshotDestroyV1)
