#include "xl6_x509_path_internal.h"
#include "src/api/xl_foreign_error.h"
#include <stdio.h>

static xx509result critical(const xx509cert* certificate,const xx509ext* extension,size_t depth,void* raw)
{
    XL6_X509PathCallV1* call=raw;
    if(call->failed)return X509_ERROR;
    xx509result status=call->procedure(certificate,extension,depth,call->data);
    if(status==X509_ERROR || xrtGetError() || (status!=X509_DONE && status!=X509_VALUE)){
        if(status!=X509_ERROR && status!=X509_DONE && status!=X509_VALUE)
            xl6X509FailV1(XERR_STATE,"critical callback returned an invalid result");
        call->failed=true;call->error=xrtTakeError();xrtSetErrorTake(xrtErrorRef(call->error));return X509_ERROR;
    }
    return status;
}
void xl6X509PathCallBeginV1(XL6_X509PathCallV1* call,const xx509pathconfig* source,xx509pathconfig* active)
{
    *call=(XL6_X509PathCallV1){.procedure=source->Critical,.data=source->UserData};*active=*source;
    if(call->procedure){active->Critical=critical;active->UserData=call;}
}
bool xl6X509PathCallEndV1(XL6_X509PathCallV1* call,xx509check check)
{
    xerror* error=xrtTakeError();
    if(call->failed){xrtErrorFree(error);xl6ErrorScopePropagateFailure((xl6_error_result){false,call->error});call->error=NULL;return false;}
    if(error || check==X509_CHECK_ERROR){xl6ErrorScopePropagateFailure((xl6_error_result){false,error});return false;}
    return true;
}
bool xl6X509PathDecisionV1(XL6_X509PathOutcomeV1* result,xx509check check,const xx509pathdecision* decision,const char* operation)
{
    if(check==X509_CHECK_VALID){
        if(!result->path || decision->Reason || decision->Depth!=SIZE_MAX || decision->Message)
            return xl6X509FailV1(XERR_STATE,"native path returned an invalid successful decision");
        result->status=XL6_X509_PATH_VALID_V1;result->depth=-1;return true;
    }
    if(check!=X509_CHECK_REJECTED && check!=X509_CHECK_UNSUPPORTED)
        return xl6X509FailV1(XERR_STATE,"native path returned an unknown result");
    if(!decision->Reason || !decision->Message || (decision->Depth!=SIZE_MAX && decision->Depth>INT64_MAX))
        return xl6X509FailV1(XERR_STATE,"native path returned an invalid completed decision");
    result->status=check==X509_CHECK_REJECTED?XL6_X509_PATH_REJECTED_V1:XL6_X509_PATH_UNSUPPORTED_V1;
    result->code=decision->Reason;result->depth=decision->Depth==SIZE_MAX?-1:(int64_t)decision->Depth;
    char data[48];xerrordesc description={0};
    description.Kind=check==X509_CHECK_REJECTED?XERR_PROTOCOL:XERR_UNSUPPORTED;
    description.Domain="xrt.x509";description.Code=(int32)decision->Reason;
    description.Operation=operation;description.Message=decision->Message;
    if(decision->Depth!=SIZE_MAX){(void)snprintf(data,sizeof(data),"depth=%llu",(unsigned long long)decision->Depth);description.Data=data;}
    /* Display text is owned and never controls validation/search decisions. */
    result->diagnostic=xrtErrorBuild(&description);if(!result->diagnostic)return false;
    xl6X509PathDestroyV1(result->path);result->path=NULL;return true;
}
