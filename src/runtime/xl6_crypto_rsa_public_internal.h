#ifndef XL6_CRYPTO_RSA_PUBLIC_INTERNAL_H
#define XL6_CRYPTO_RSA_PUBLIC_INTERNAL_H
#include "xl6_crypto_internal.h"
typedef struct XL6_CryptoRsaPublicV1 {
    size_t allocation;
    xrsapublickey key;
    uint8 data[];
} XL6_CryptoRsaPublicV1;
static inline bool xl6CryptoRsaRead(const xvalue* value,xbytesview* view)
{
    if (!xl6CryptoMaterial(value,view)) return false;
    if (view->Size>XRT_RSA_MAX_MODULUS_SIZE) {xrtSetErrorKind(XERR_RANGE);return false;}
    return true;
}
static inline const XL6_CryptoRsaPublicV1* xl6CryptoRsaPublicChecked(void* raw)
{
    if (!raw) xrtSetErrorKind(XERR_ARGUMENT);
    return raw;
}
static inline size_t xl6CryptoRsaBits(const xrsapublickey* key)
{
    size_t result=key->ModulusSize*8;uint8 first=((const uint8*)key->Modulus)[0];
    while (!(first&0x80)) {first=(uint8)(first<<1);--result;}
    return result;
}
static inline bool xl6CryptoRsaHash(int64_t algorithm,xcryptohash* result,size_t* size)
{
    if (algorithm<1 || algorithm>5) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    *result=(xcryptohash)algorithm;*size=xrtCryptoHashSize(*result);return true;
}
static inline bool xl6CryptoRsaDigest(const xvalue* value,int64_t algorithm,xcryptohash* hash,xbytesview* view)
{
    size_t size;
    if (!xl6CryptoRsaHash(algorithm,hash,&size) || !xl6CryptoMaterial(value,view)) return false;
    if (view->Size!=size) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return true;
}
static inline bool xl6CryptoRsaSignature(const xvalue* value,size_t size,xbytesview* view)
{
    if (!xl6CryptoMaterial(value,view)) return false;
    if (view->Size!=size) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    return true;
}
/* Resident-only owned-copy helper; no borrowed native key crosses the ABI. */
XL6_TEXT_API void* xl6CryptoRsaPublicCopyV1(const xrsapublickey*);
#endif
