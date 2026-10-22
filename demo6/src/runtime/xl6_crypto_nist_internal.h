#ifndef XL6_CRYPTO_NIST_INTERNAL_H
#define XL6_CRYPTO_NIST_INTERNAL_H
#include "xl6_crypto_internal.h"

typedef struct XL6_CryptoP256KeyV1 {uint8 privateKey[32],publicKey[65];} XL6_CryptoP256KeyV1;
typedef struct XL6_CryptoP384KeyV1 {uint8 privateKey[48],publicKey[97];} XL6_CryptoP384KeyV1;
static inline bool xl6CryptoNistMaterial(const xvalue* value,size_t size,xbytesview* view)
{
    if (!xl6CryptoMaterial(value,view)) return false;
    if (view->Size!=size) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return true;
}
static inline bool xl6CryptoNistDigest(const xvalue* value,xbytesview* view)
{
    if (!xl6CryptoMaterial(value,view)) return false;
    if (!view->Size) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return true;
}
/* Only these algorithms have native RFC6979 backends. Never infer the hash
 * algorithm from digest length (different algorithms may share a width). */
static inline bool xl6CryptoNistSignDigest(const xvalue* value,int64_t algorithm,xbytesview* view)
{
    size_t size;
    switch (algorithm) {
        case XCRYPTO_HASH_SHA256:size=32;break;
        case XCRYPTO_HASH_SHA384:size=48;break;
        case XCRYPTO_HASH_SHA512:size=64;break;
        default:xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    return xl6CryptoNistMaterial(value,size,view);
}
#endif
