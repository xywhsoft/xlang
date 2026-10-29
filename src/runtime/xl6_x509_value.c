#include "xl6_x509_internal.h"

bool xl6X509FailV1(xerrkind kind,const char* message)
{
    if(!xrtGetError()) {
        xerror* error=xrtErrorCreate(kind,"xlang.x509",1,message);
        if(error)xrtSetErrorTake(error);
        else if(!xrtGetError())xrtSetErrorKind(XERR_MEMORY);
    }
    return false;
}
bool xl6X509OutcomeV1(bool ok)
{
    if(!ok && !xrtGetError())xl6X509FailV1(XERR_STATE,"X.509 operation failed without a diagnostic");
    return ok && !xrtGetError();
}
bool xl6X509SpanV1(xbytesview input)
{
    if(!input.Data || !input.Size)return xl6X509FailV1(XERR_ARGUMENT,"expected complete nonempty X.509 DER");
    if(input.Size>INT64_MAX)return xl6X509FailV1(XERR_RANGE,"X.509 DER is too large for a language value");
    return true;
}
bool xl6X509LimitV1(size_t actual,int64_t limit,const char* message)
{
    if(limit<0)return xl6X509FailV1(XERR_ARGUMENT,"negative X.509 byte budget");
    return !limit || actual<=(uint64_t)limit || xl6X509FailV1(XERR_RANGE,message);
}
bool xl6X509InputV1(const xvalue* value,xbytesview* input)
{
    return xl6X509OutcomeV1(xrtValueGetBytes(value,input) && xl6X509SpanV1(*input));
}
void xl6X509FreeV1(void* data)
{
    xerror* prior=xrtTakeError();xrtFree(data);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
xvalue* xl6X509BytesV1(xbytesview input)
{
    if((input.Size && !input.Data) || input.Size>INT64_MAX) {
        xl6X509FailV1(XERR_STATE,"invalid owned X.509 field view");return NULL;
    }
    xvalue* result=xrtValueBytes(input);
    if(xl6X509OutcomeV1(result!=NULL))return result;
    xerror* prior=xrtTakeError();xrtValueRelease(result);xrtClearError();xrtSetErrorTake(prior);return NULL;
}
