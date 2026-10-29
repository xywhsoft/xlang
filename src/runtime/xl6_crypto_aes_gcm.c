#include "xl6_crypto_internal.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* The expanded key is immutable after construction; parallel readers do not
 * mutate it. No language values, callbacks, or generated code are retained. */
void xl6CryptoAesGcmDestroyV1(void* raw)
{if (raw) {xrtAesGcmClear(raw);xl6CryptoFreeSecret(raw,sizeof(xaesgcm));}}
void* xl6CryptoAesGcmCreateV1(const xvalue* keyValue)
{
    xbytesview key;if (!xl6CryptoMaterial(keyValue,&key)) return NULL;
    if (key.Size!=16 && key.Size!=24 && key.Size!=32) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xaesgcm* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    if (!xrtAesGcmInit(self,key.Data,key.Size,16)) {xl6CryptoAesGcmDestroyV1(self);return NULL;}
    return self;
}
static xvalue* crypt(void* raw,xbytesview input,const xvalue* nonceValue,
    const xvalue* aadValue,int64_t aadSize,bool seal)
{
    xbytesview nonce,aad;
    if (!raw) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (!xl6CryptoMaterial(nonceValue,&nonce) || !xl6BufferBytesInput(aadValue,aadSize,&aad)) return NULL;
    if (nonce.Size!=12) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if ((!seal && input.Size<16) || (seal && input.Size>(size_t)INT64_MAX-16)) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    size_t n=seal?input.Size+16:input.Size-16;
    void* output=xrtMalloc(n?n:1);if (!output) return NULL;
    bool ok=seal?xrtAesGcmSeal(raw,nonce.Data,nonce.Size,aad.Data,aad.Size,input.Data,input.Size,output,n)
        :xrtAesGcmOpen(raw,nonce.Data,nonce.Size,aad.Data,aad.Size,input.Data,input.Size,output,n);
    return xl6CryptoTakeSecret(output,n,ok);
}
xvalue* xl6CryptoAesGcmSealV1(void* self,const xvalue* data,const xvalue* nonce,const xvalue* aad,int64_t size,int64_t aadSize)
{xbytesview input;return xl6BufferBytesInput(data,size,&input)?crypt(self,input,nonce,aad,aadSize,true):NULL;}
xvalue* xl6CryptoAesGcmSealTextV1(void* self,xstrview text,const xvalue* nonce,const xvalue* aad,int64_t size,int64_t aadSize)
{return xl6TextSelectRaw(&text,size)?crypt(self,(xbytesview){(const uint8*)text.Data,text.Size},nonce,aad,aadSize,true):NULL;}
xvalue* xl6CryptoAesGcmOpenV1(void* self,const xvalue* data,const xvalue* nonce,const xvalue* aad,int64_t size,int64_t aadSize)
{xbytesview input;return xl6BufferBytesInput(data,size,&input)?crypt(self,input,nonce,aad,aadSize,false):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6CryptoAesGcmHandlePolicyV1Get,xl6CryptoAesGcmDestroyV1)

/* Synchronous readonly span: the caller keeps the input owner alive. */
void* xl6CryptoAesGcmCreateSpanV1(xbytesview keyValue)
{
    xbytesview key;if (!xl6BufferSpanInput(keyValue,-1,&key)) return NULL;
    if (key.Size!=16 && key.Size!=24 && key.Size!=32) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xaesgcm* self=xrtCalloc(1,sizeof(*self));if (!self) return NULL;
    if (!xrtAesGcmInit(self,key.Data,key.Size,16)) {xl6CryptoAesGcmDestroyV1(self);return NULL;}
    return self;
}
