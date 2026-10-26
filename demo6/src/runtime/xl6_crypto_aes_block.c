#include "xl6_crypto_aes_block_internal.h"
#include "xl6_crypto_abi.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Immutable expanded key. This primitive processes exactly one block, without
 * padding, an IV, a mode of operation, or authentication. */
void xl6CryptoAesBlockDestroyV1(void* raw)
{
    if (raw) {
        xerror* error=xrtTakeError();
        xrtAesClear(raw);
        xl6CryptoFreeSecret(raw,sizeof(xaes));
        xrtClearError();xrtSetErrorTake(error);
    }
}
void* xl6CryptoAesBlockCreateSpanV1(xbytesview key)
{
    if (!xl6BufferSpanInput(key,-1,&key)) return NULL;
    if (key.Size!=16 && key.Size!=24 && key.Size!=32) {
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    xaes* self=xrtCalloc(1,sizeof(*self));
    if (!self) return NULL;
    if (!xrtAesInit(self,key.Data,key.Size)) {
        xl6CryptoAesBlockDestroyV1(self);return NULL;
    }
    return self;
}
void* xl6CryptoAesBlockCreateV1(const xvalue* keyValue)
{
    xbytesview key;
    return xl6CryptoMaterial(keyValue,&key)?xl6CryptoAesBlockCreateSpanV1(key):NULL;
}
xvalue* xl6CryptoAesBlockEncryptSpanV1(void* self,xbytesview input)
{return xl6BufferSpanInput(input,-1,&input)?xl6CryptoAesEncryptBlock(self,input):NULL;}
xvalue* xl6CryptoAesBlockDecryptSpanV1(void* self,xbytesview input)
{return xl6BufferSpanInput(input,-1,&input)?xl6CryptoAesDecryptBlock(self,input):NULL;}
xvalue* xl6CryptoAesBlockEncryptV1(void* self,const xvalue* value)
{xbytesview input;return xl6CryptoMaterial(value,&input)?xl6CryptoAesEncryptBlock(self,input):NULL;}
xvalue* xl6CryptoAesBlockDecryptV1(void* self,const xvalue* value)
{xbytesview input;return xl6CryptoMaterial(value,&input)?xl6CryptoAesDecryptBlock(self,input):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoAesBlockHandlePolicyV1Get,xl6CryptoAesBlockDestroyV1)
