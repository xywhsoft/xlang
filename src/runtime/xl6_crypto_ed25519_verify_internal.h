#ifndef XL6_CRYPTO_ED25519_VERIFY_INTERNAL_H
#define XL6_CRYPTO_ED25519_VERIFY_INTERNAL_H
#include "xl6_crypto_ed25519_internal.h"

/* Only the exact native verification rejection is a boolean false. OOM,
 * unrelated domains, invalid API arguments and internal failures stay checked. */
static inline bool xl6CryptoEd25519Rejection(const xerror* error)
{
    const char* domain=xrtErrorDomain(error);
    if (!domain || strcmp(domain,"xrt.crypto")) return false;
    return (xrtErrorKind(error)==XERR_VALUE && xrtErrorCode(error)==XCRYPTO_ERROR_KEY) ||
        (xrtErrorKind(error)==XERR_PROTOCOL && xrtErrorCode(error)==XCRYPTO_ERROR_SIGNATURE);
}
static inline bool xl6CryptoEd25519VerifyFinish(bool ok,xerror* previous)
{
    xerror* current=xrtTakeError();
    if (!ok && xl6CryptoEd25519Rejection(current)) {xrtErrorFree(current);current=NULL;}
    if (previous) {if (current!=previous)xrtErrorFree(current);current=previous;}
    xrtSetErrorTake(current);return ok;
}
#endif
