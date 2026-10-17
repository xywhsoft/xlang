#include "xl6_crypto_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

void xl6CryptoChaChaAeadDestroyV1(void* raw)
{xl6CryptoFreeSecret(raw,32);}
void* xl6CryptoChaChaAeadCreateV1(const xvalue* keyValue)
{
    xbytesview key;if (!xl6CryptoMaterial(keyValue,&key)) return NULL;
    if (key.Size!=32) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    void* self=xrtMalloc(32);if (self) memcpy(self,key.Data,32);return self;
}
static xvalue* crypt(void* key,xbytesview input,const xvalue* nonceValue,
    const xvalue* aadValue,int64_t aadSize,bool seal)
{
    xbytesview nonce,aad;
    if (!key) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6CryptoMaterial(nonceValue,&nonce) || !xl6BufferBytesInput(aadValue,aadSize,&aad)) return NULL;
    if (nonce.Size!=12) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if ((!seal && input.Size<16) || (seal && input.Size>(size_t)INT64_MAX-16)) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    size_t n=seal?input.Size+16:input.Size-16;
    void* output=xrtMalloc(n?n:1);if (!output) return NULL;
    bool ok=seal?xrtChaCha20Poly1305Seal(key,nonce.Data,aad.Data,aad.Size,input.Data,input.Size,output,n)
        :xrtChaCha20Poly1305Open(key,nonce.Data,aad.Data,aad.Size,input.Data,input.Size,output,n);
    return xl6CryptoTakeSecret(output,n,ok);
}
xvalue* xl6CryptoChaChaAeadSealV1(void* self,const xvalue* data,const xvalue* nonce,const xvalue* aad,int64_t size,int64_t aadSize)
{xbytesview input;return xl6BufferBytesInput(data,size,&input)?crypt(self,input,nonce,aad,aadSize,true):NULL;}
xvalue* xl6CryptoChaChaAeadSealTextV1(void* self,xstrview text,const xvalue* nonce,const xvalue* aad,int64_t size,int64_t aadSize)
{return xl6TextSelectRaw(&text,size)?crypt(self,(xbytesview){(const uint8*)text.Data,text.Size},nonce,aad,aadSize,true):NULL;}
xvalue* xl6CryptoChaChaAeadOpenV1(void* self,const xvalue* data,const xvalue* nonce,const xvalue* aad,int64_t size,int64_t aadSize)
{xbytesview input;return xl6BufferBytesInput(data,size,&input)?crypt(self,input,nonce,aad,aadSize,false):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoChaChaAeadHandlePolicyV1Get,xl6CryptoChaChaAeadDestroyV1)
