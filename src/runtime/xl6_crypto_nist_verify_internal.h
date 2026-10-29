#ifndef XL6_CRYPTO_NIST_VERIFY_INTERNAL_H
#define XL6_CRYPTO_NIST_VERIFY_INTERNAL_H
#include "xl6_crypto_nist_internal.h"
/* Typed native rejection only. Resource/internal failures remain checked and
 * cleanup must never replace an error already pending before verification. */
static inline bool xl6CryptoNistRejection(const xerror* error)
{
    const char* domain=xrtErrorDomain(error);
    return domain && !strcmp(domain,"xrt.crypto") &&
        ((xrtErrorKind(error)==XERR_VALUE && xrtErrorCode(error)==XCRYPTO_ERROR_KEY) ||
         (xrtErrorKind(error)==XERR_PROTOCOL && xrtErrorCode(error)==XCRYPTO_ERROR_SIGNATURE));
}
static inline bool xl6CryptoNistVerifyFinish(bool ok,xerror* previous)
{
    xerror* current=xrtTakeError();
    if (!ok && xl6CryptoNistRejection(current)) {xrtErrorFree(current);current=NULL;}
    if (previous) {if (current!=previous)xrtErrorFree(current);current=previous;}
    xrtSetErrorTake(current);return ok;
}
#endif
