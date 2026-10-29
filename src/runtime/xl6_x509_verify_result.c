#include "xl6_x509_verify_internal.h"

static bool signature_envelope(const xerror* error)
{
    const char* domain=xrtErrorDomain(error);xerrkind kind=xrtErrorKind(error);
    return domain && !strcmp(domain,"xrt.x509") && xrtErrorCode(error)==X509_ERROR_SIGNATURE &&
        (kind==XERR_PROTOCOL || kind==XERR_UNSUPPORTED);
}
static bool known_causes(const xerror* error,int64_t* result)
{
    for(const xerror* cause=xrtErrorCause(error);cause;cause=xrtErrorCause(cause)) {
        if(signature_envelope(cause)) {
            if(xrtErrorKind(cause)==XERR_UNSUPPORTED)*result=XL6_X509_SIGNATURE_UNSUPPORTED_V1;
            continue;
        }
        const char* domain=xrtErrorDomain(cause);
        if(domain && !strcmp(domain,"xrt.crypto") &&
            ((xrtErrorKind(cause)==XERR_PROTOCOL && xrtErrorCode(cause)==XCRYPTO_ERROR_SIGNATURE) ||
             (xrtErrorKind(cause)==XERR_VALUE && xrtErrorCode(cause)==XCRYPTO_ERROR_KEY)))continue;
        return false;
    }
    return true;
}
int64_t xl6X509SignatureFinishV1(bool ok,xerror* previous,bool strict)
{
    xerror* current=xrtTakeError();int64_t result=XL6_X509_SIGNATURE_ERROR_V1;
    if(ok && !current)result=XL6_X509_SIGNATURE_VALID_V1;
    else if(!ok && !strict && signature_envelope(current)) {
        int64_t status=xrtErrorKind(current)==XERR_UNSUPPORTED?XL6_X509_SIGNATURE_UNSUPPORTED_V1:XL6_X509_SIGNATURE_REJECTED_V1;
        if(known_causes(current,&status)){result=status;xrtErrorFree(current);current=NULL;}
    } else if(!current) {
        xl6X509FailV1(XERR_STATE,"signature verification failed without a diagnostic");current=xrtTakeError();
    }
    if(previous) {if(current!=previous)xrtErrorFree(current);current=previous;}
    xrtSetErrorTake(current);return result;
}
