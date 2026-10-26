#ifndef XL6_CRYPTO_AES_BLOCK_INTERNAL_H
#define XL6_CRYPTO_AES_BLOCK_INTERNAL_H
#include "xl6_crypto_internal.h"
/* Shared synchronous adaptation, inlined independently into bytes and Buffer
 * units. No operation-code dispatch or extra cross-adapter call. */
static inline xvalue* xl6CryptoAesEncryptBlock(void* self,xbytesview input)
{
    uint8 output[XRT_AES_BLOCK_SIZE];
    if (!self || input.Size!=sizeof(output)) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    bool ok=xrtAesEncrypt(self,input.Data,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
static inline xvalue* xl6CryptoAesDecryptBlock(void* self,xbytesview input)
{
    uint8 output[XRT_AES_BLOCK_SIZE];
    if (!self || input.Size!=sizeof(output)) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    bool ok=xrtAesDecrypt(self,input.Data,output);
    xvalue* result=ok?xl6CryptoCopySecret(output,sizeof(output)):NULL;
    xrtSecureZero(output,sizeof(output));return result;
}
#endif
