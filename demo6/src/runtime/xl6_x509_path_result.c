#include "xl6_x509_path_internal.h"
#include "src/api/xl_foreign_error.h"

static bool domainIs(const xerror* error,const char* expected,size_t size)
{
    xstrview domain=xrtErrorDomainView(error);
    return domain.Size==size && domain.Data && !memcmp(domain.Data,expected,size);
}
static bool verdict(const xerror* error,int64_t* status)
{
    if(!domainIs(error,"xrt.x509",8))return false;
    xerrkind kind=xrtErrorKind(error);int32_t code=xrtErrorCode(error);
    if(kind==XERR_UNSUPPORTED && (code==X509_ERROR_SIGNATURE || code==X509_ERROR_NAME_CONSTRAINTS)) {
        *status=XL6_X509_PATH_UNSUPPORTED_V1;return true;
    }
    if(kind!=XERR_PROTOCOL)return false;
    switch(code) {
    case X509_ERROR_SIGNATURE:case X509_ERROR_ISSUER:case X509_ERROR_PATH:
    case X509_ERROR_PATH_CONSTRAINT:case X509_ERROR_CRITICAL_EXTENSION:
    case X509_ERROR_PURPOSE:case X509_ERROR_NAME_CONSTRAINTS:return true;
    default:return false;
    }
}
bool xl6X509PathClassifyV1(const xerror* error,int64_t* status)
{
    if(!error || !status)return false;
    int64_t selected=XL6_X509_PATH_REJECTED_V1;
    if(!verdict(error,&selected))return false;
    for(const xerror* cause=xrtErrorCause(error);cause;cause=xrtErrorCause(cause)) {
        if(verdict(cause,&selected))continue;
        if(domainIs(cause,"xrt.crypto",10) &&
            ((xrtErrorKind(cause)==XERR_PROTOCOL && xrtErrorCode(cause)==XCRYPTO_ERROR_SIGNATURE) ||
             (xrtErrorKind(cause)==XERR_VALUE && xrtErrorCode(cause)==XCRYPTO_ERROR_KEY)))continue;
        return false; /* Includes wrapped OOM, arguments and unknown domains. */
    }
    *status=selected;return true;
}
void xl6X509PathOutcomeDestroyV1(void* raw)
{
    XL6_X509PathOutcomeV1* self=raw;
    if(!self)return;
    xerror* prior=xrtTakeError();
    xrtErrorFree(self->diagnostic);xl6X509PathDestroyV1(self->path);xl6X509FreeV1(self);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
static const XL6_X509PathOutcomeV1* checked(void* raw)
{if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing path outcome");return raw;}
int64_t xl6X509PathOutcomeStatusV1(void* raw)
{const XL6_X509PathOutcomeV1* self=checked(raw);return self?self->status:0;}
int64_t xl6X509PathOutcomeCodeV1(void* raw)
{const XL6_X509PathOutcomeV1* self=checked(raw);return self?self->code:0;}
bool xl6X509PathOutcomeRequireV1(void* raw)
{
    const XL6_X509PathOutcomeV1* self=checked(raw);
    if(!self || xrtGetError())return false;
    if(self->status==XL6_X509_PATH_VALID_V1)return true;
    if(!self->diagnostic)return xl6X509FailV1(XERR_STATE,"negative path outcome has no diagnostic");
    xrtSetErrorTake(xrtErrorRef(self->diagnostic));return false;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509PathOutcomeHandlePolicyV1Get,xl6X509PathOutcomeDestroyV1)
