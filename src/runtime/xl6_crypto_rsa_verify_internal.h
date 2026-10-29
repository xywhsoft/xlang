#ifndef XL6_CRYPTO_RSA_VERIFY_INTERNAL_H
#define XL6_CRYPTO_RSA_VERIFY_INTERNAL_H
#include "xl6_crypto_rsa_public_internal.h"
static inline bool xl6CryptoRsaRejection(const xerror* error)
{
    const char* domain=xrtErrorDomain(error);
    return domain && !strcmp(domain,"xrt.crypto") &&
        ((xrtErrorKind(error)==XERR_VALUE && xrtErrorCode(error)==XCRYPTO_ERROR_KEY) ||
         (xrtErrorKind(error)==XERR_PROTOCOL && xrtErrorCode(error)==XCRYPTO_ERROR_SIGNATURE));
}
static inline bool xl6CryptoRsaVerifyFinish(bool ok,xerror* previous)
{
    xerror* current=xrtTakeError();
    if (!ok && xl6CryptoRsaRejection(current)) {xrtErrorFree(current);current=NULL;}
    if (previous) {if (current!=previous)xrtErrorFree(current);current=previous;}
    xrtSetErrorTake(current);return ok;
}
#endif
