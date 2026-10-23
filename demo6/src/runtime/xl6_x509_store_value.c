#include "xl6_x509_store_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509StoreDestroyV1(void* raw)
{
    XL6_X509StoreV1* self=raw;if(!self)return;
    xerror* error=xrtTakeError();
    /* Only a private, inaccessible lock exists. Last-reference retirement
     * cannot overlap a live owning call or encounter a legally held lock. */
    if(self->initialized && !xl6X509OutcomeV1(xrtMutexUnit(&self->lock))){
        if(error){xrtClearError();xrtSetErrorTake(error);}return;
    }
    xl6X509StoreSnapshotDestroyV1(self->current);xl6X509FreeV1(self);
    if(error){xrtClearError();xrtSetErrorTake(error);}
}
XL6_X509StoreV1* xl6X509StoreValueTakeV1(XL6_X509StoreVersionV1* version)
{
    if(!version)return NULL;
    XL6_X509StoreV1* self=xrtCalloc(1,sizeof(*self));
    if(!self){xl6X509StoreSnapshotDestroyV1(version);return NULL;}
    self->current=version;
    if(!xl6X509OutcomeV1(xrtMutexInit(&self->lock))){xl6X509StoreDestroyV1(self);return NULL;}
    self->initialized=true;return self;
}
void* xl6X509StoreCreateV1(void)
{
    if(xrtGetError())return NULL;
    xx509store* value=xrtX509StoreCreate();
    return value?xl6X509StoreValueTakeV1(xl6X509StoreVersionTakeV1(&value,0)):NULL;
}
XL6_X509StoreVersionV1* xl6X509StoreAcquireV1(void* raw)
{
    XL6_X509StoreV1* self=raw;
    if(xrtGetError())return NULL;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing trust store");return NULL;}
    if(!xl6X509OutcomeV1(xrtMutexLock(&self->lock)))return NULL;
    XL6_X509StoreVersionV1* version=xl6X509StoreVersionRefV1(self->current);
    xerror* error=xrtTakeError();bool unlocked=xl6X509OutcomeV1(xrtMutexUnlock(&self->lock));
    if(error){xrtClearError();xrtSetErrorTake(error);}
    if(!unlocked || xrtGetError()){xl6X509StoreSnapshotDestroyV1(version);return NULL;}
    return version;
}
void* xl6X509StoreSnapshotV1(void* raw){return xl6X509StoreAcquireV1(raw);}
void* xl6X509StoreCloneV1(void* raw){return xl6X509StoreValueTakeV1(xl6X509StoreAcquireV1(raw));}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509StoreHandlePolicyV1Get,xl6X509StoreDestroyV1)
