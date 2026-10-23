#include "xl6_x509_path_internal.h"
#include "src/api/xl_foreign_error.h"

typedef struct CriticalCall {
    xx509criticalproc procedure;
    void* data;
    bool failed;
    xerror* error;
} CriticalCall;
static xx509result critical(const xx509cert* certificate,const xx509ext* extension,size_t depth,void* raw)
{
    CriticalCall* call=raw;
    if(call->failed)return X509_ERROR;
    xx509result status=call->procedure(certificate,extension,depth,call->data);
    if(status==X509_ERROR || xrtGetError() || (status!=X509_DONE && status!=X509_VALUE)) {
        if(status!=X509_ERROR && status!=X509_DONE && status!=X509_VALUE)
            xl6X509FailV1(XERR_STATE,"critical callback returned an invalid result");
        call->failed=true;call->error=xrtTakeError();
        xrtSetErrorTake(xrtErrorRef(call->error));return X509_ERROR;
    }
    return status;
}

static bool add(size_t* total,size_t size)
{
    if(size>SIZE_MAX-*total)return xl6X509FailV1(XERR_RANGE,"path snapshot allocation overflow");
    *total+=size;return true;
}
static XL6_X509PathV1* snapshot(void* const* certificates,size_t count,const XL6_X509AnchorV1* anchor,
    const xx509pathconfig* policy,int64_t maxBytes)
{
    size_t total=sizeof(XL6_X509PathV1),inputBytes=policy->Purpose.Size;
    if(count>(SIZE_MAX-total)/(sizeof(xx509cert)+sizeof(xx509cert*))) {
        xl6X509FailV1(XERR_RANGE,"path snapshot index overflow");return NULL;
    }
    total+=count*(sizeof(xx509cert)+sizeof(xx509cert*));
    if(!add(&inputBytes,anchor->certificate.Raw.Size))return NULL;
    for(size_t i=0;i<count;++i) {
        if(!certificates[i]){xl6X509FailV1(XERR_ARGUMENT,"path contains a missing certificate");return NULL;}
        xbytesview raw=((const XL6_X509CertificateV1*)certificates[i])->view.Raw;
        if(!xl6X509SpanV1(raw) || !add(&inputBytes,raw.Size))return NULL;
    }
    if(!xl6X509LimitV1(inputBytes,maxBytes,"path input exceeds byte budget") || !add(&total,inputBytes))return NULL;
    XL6_X509PathV1* self=xrtCalloc(1,total);if(!self)return NULL;
    xrtAtomic64Init(&self->references,1);
    self->count=count;self->policy=*policy;
    /* Only this synchronous call uses the borrowed callback. A published
     * path never retains UserData, a closure, or a generated code address. */
    self->policy.Critical=NULL;self->policy.UserData=NULL;
    const xx509cert** index=(const xx509cert**)(self->certificates+count);
    uint8* data=(uint8*)(index+count);
    memcpy(data,anchor->certificate.Raw.Data,anchor->certificate.Raw.Size);
    bool ok=xrtX509Parse(data,anchor->certificate.Raw.Size,&self->anchorCertificate) &&
        xrtX509Anchor(&self->anchorCertificate,&self->anchor);
    data+=anchor->certificate.Raw.Size;
    for(size_t i=0;ok && i<count;++i) {
        xbytesview raw=((const XL6_X509CertificateV1*)certificates[i])->view.Raw;
        memcpy(data,raw.Data,raw.Size);ok=xrtX509Parse(data,raw.Size,&self->certificates[i]);
        data+=raw.Size;index[i]=&self->certificates[i];
    }
    if(policy->Purpose.Size){memcpy(data,policy->Purpose.Data,policy->Purpose.Size);self->policy.Purpose=(xbytesview){data,policy->Purpose.Size};}
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509PathDestroyV1(self);return NULL;
}
void* xl6X509PathValidateCreateV1(void* const* certificates,size_t count,void* anchor,const xx509pathconfig* config,
    int64_t maxCertificates,int64_t maxBytes)
{
    if(xrtGetError())return NULL;
    if(!certificates || !count || !anchor) {
        xl6X509FailV1(XERR_ARGUMENT,"path needs ordered certificates and an independent explicit anchor");return NULL;
    }
    if(!xl6X509LimitV1(count,maxCertificates,"path exceeds certificate budget") ||
        !xl6X509LimitV1(config?config->Purpose.Size:0,maxBytes,"path purpose exceeds byte budget") ||
        !xl6X509PathPolicyValidV1(config))return NULL;
    XL6_X509PathOutcomeV1* result=xrtCalloc(1,sizeof(*result));if(!result)return NULL;
    result->path=snapshot(certificates,count,anchor,config,maxBytes);
    if(!result->path){xl6X509PathOutcomeDestroyV1(result);return NULL;}
    const xx509cert** index=(const xx509cert**)(result->path->certificates+count);
    xx509pathconfig policy=result->path->policy;
    CriticalCall call={.procedure=config->Critical,.data=config->UserData};
    if(config->Critical){policy.Critical=critical;policy.UserData=&call;}
    bool valid=xrtX509PathValidate(index,count,&result->path->anchor,&policy);
    xerror* error=xrtTakeError();
    if(call.failed) {
        xrtErrorFree(error);xl6X509PathOutcomeDestroyV1(result);
        /* A missing diagnostic is still a failed checked callback, never a
         * normal unsupported-extension verdict manufactured by native code. */
        xl6ErrorScopePropagateFailure((xl6_error_result){false,call.error});return NULL;
    }
    if(valid && !error){result->status=XL6_X509_PATH_VALID_V1;return result;}
    int64_t status=0;
    if(!valid && xl6X509PathClassifyV1(error,&status)) {
        result->status=status;result->code=xrtErrorCode(error);result->diagnostic=error;
        xl6X509PathDestroyV1(result->path);result->path=NULL;return result;
    }
    xrtSetErrorTake(error);xl6X509FailV1(XERR_STATE,"path validation failed without a usable diagnostic");
    xl6X509PathOutcomeDestroyV1(result);return NULL;
}
