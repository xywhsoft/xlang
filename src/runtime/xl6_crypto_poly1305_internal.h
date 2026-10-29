#ifndef XL6_CRYPTO_POLY1305_INTERNAL_H
#define XL6_CRYPTO_POLY1305_INTERNAL_H
#include "xl6_crypto_internal.h"
static inline xvalue* xl6CryptoPoly1305Authenticate(xbytesview key,xbytesview input)
{
    uint8 output[XRT_POLY1305_TAG_SIZE];
    if (key.Size!=XRT_POLY1305_KEY_SIZE) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    bool ok=xrtPoly1305(key.Data,input.Data,input.Size,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
#endif
