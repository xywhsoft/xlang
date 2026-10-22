#ifndef XL6_CRYPTO_RSA_PRIVATE_INTERNAL_H
#define XL6_CRYPTO_RSA_PRIVATE_INTERNAL_H
#include "xl6_crypto_rsa_public_internal.h"
typedef struct XL6_CryptoRsaPrivateV1 {
    size_t allocation;
    xrsaprivatekey key;
    uint8 data[];
} XL6_CryptoRsaPrivateV1;
static inline const XL6_CryptoRsaPrivateV1* xl6CryptoRsaPrivateChecked(void* raw)
{
    if (!raw) xrtSetErrorKind(XERR_ARGUMENT);
    return raw;
}
/* Native views are rebound to the new owned allocation, never retained from
 * caller values. This also supports future explicit format import adapters. */
XL6_TEXT_API void* xl6CryptoRsaPrivateFromPartsV1(const xbytesview[8]);
#endif
