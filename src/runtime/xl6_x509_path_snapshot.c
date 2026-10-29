#include "xl6_x509_path_internal.h"
static bool add(size_t* total,size_t size)
{
    if(size>SIZE_MAX-*total)return xl6X509FailV1(XERR_RANGE,"path snapshot allocation overflow");
    *total+=size;return true;
}
XL6_X509PathV1* xl6X509PathSnapshotV1(const xx509cert* const* certificates,size_t count,const xx509cert* anchor,
    const xx509pathconfig* policy,int64_t maxBytes)
{
    if(!certificates || !count || !anchor || !policy){xl6X509FailV1(XERR_ARGUMENT,"missing validated path input");return NULL;}
    size_t total=sizeof(XL6_X509PathV1),inputBytes=policy->Purpose.Size;
    if(count>(SIZE_MAX-total)/(sizeof(xx509cert)+sizeof(xx509cert*))){xl6X509FailV1(XERR_RANGE,"path snapshot index overflow");return NULL;}
    total+=count*(sizeof(xx509cert)+sizeof(xx509cert*));
    if(!xl6X509SpanV1(anchor->Raw) || !add(&inputBytes,anchor->Raw.Size))return NULL;
    for(size_t i=0;i<count;++i){
        if(!certificates[i]){xl6X509FailV1(XERR_ARGUMENT,"path contains a missing certificate");return NULL;}
        if(!xl6X509SpanV1(certificates[i]->Raw) || !add(&inputBytes,certificates[i]->Raw.Size))return NULL;
    }
    if(!xl6X509LimitV1(inputBytes,maxBytes,"path input exceeds byte budget") || !add(&total,inputBytes))return NULL;
    XL6_X509PathV1* self=xrtCalloc(1,total);if(!self)return NULL;
    xrtAtomic64Init(&self->references,1);self->count=count;self->policy=*policy;
    self->policy.Critical=NULL;self->policy.UserData=NULL;
    const xx509cert** index=(const xx509cert**)(self->certificates+count);uint8* data=(uint8*)(index+count);
    memcpy(data,anchor->Raw.Data,anchor->Raw.Size);
    bool ok=xrtX509Parse(data,anchor->Raw.Size,&self->anchorCertificate) && xrtX509Anchor(&self->anchorCertificate,&self->anchor);
    data+=anchor->Raw.Size;
    for(size_t i=0;ok && i<count;++i){
        xbytesview raw=certificates[i]->Raw;memcpy(data,raw.Data,raw.Size);
        ok=xrtX509Parse(data,raw.Size,&self->certificates[i]);data+=raw.Size;index[i]=&self->certificates[i];
    }
    if(policy->Purpose.Size){memcpy(data,policy->Purpose.Data,policy->Purpose.Size);self->policy.Purpose=(xbytesview){data,policy->Purpose.Size};}
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509PathDestroyV1(self);return NULL;
}
