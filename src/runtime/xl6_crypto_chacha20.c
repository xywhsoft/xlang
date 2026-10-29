#include "xl6_crypto_internal.h"
#include "xl6_crypto_abi.h"

/* Raw IETF ChaCha20 is deliberately an advanced primitive, not authenticated
 * encryption. All entry points share the same material and counter contract. */
static xvalue* crypt(xbytesview key,xbytesview nonce,xbytesview data,int64_t counter)
{
    if (key.Size!=32 || nonce.Size!=12) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (counter<0 || (uint64_t)counter>UINT32_MAX) {xrtSetErrorKind(XERR_RANGE);return NULL;}
    /* Check before allocating. The last block is allowed; wrapping is not. */
    uint64_t blocks=(uint64_t)(data.Size/64)+(data.Size%64!=0);
    if (blocks>UINT64_C(0x100000000)-(uint64_t)counter) {
        xrtSetErrorKind(XERR_RANGE);return NULL;
    }
    void* output=xrtMalloc(data.Size?data.Size:1);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,data.Size,
        xrtChaCha20(key.Data,nonce.Data,(uint32)counter,data.Data,output,data.Size));
}
xvalue* xl6CryptoChaCha20TextV1(const xvalue* keyValue,const xvalue* nonceValue,
    xstrview text,int64_t counter,int64_t size)
{
    xbytesview key,nonce;
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6CryptoMaterial(nonceValue,&nonce) ||
        !xl6TextSelectRaw(&text,size)) return NULL;
    return crypt(key,nonce,(xbytesview){(const uint8*)text.Data,text.Size},counter);
}
xvalue* xl6CryptoChaCha20BytesV1(const xvalue* keyValue,const xvalue* nonceValue,
    const xvalue* dataValue,int64_t counter,int64_t size)
{
    xbytesview key,nonce,data;
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6CryptoMaterial(nonceValue,&nonce) ||
        !xl6BufferBytesInput(dataValue,size,&data)) return NULL;
    return crypt(key,nonce,data,counter);
}
xvalue* xl6CryptoChaCha20BufferV1(const xvalue* keyValue,const xvalue* nonceValue,
    void* buffer,int64_t counter,int64_t size)
{
    xbytesview key,nonce,data;
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6CryptoMaterial(nonceValue,&nonce) ||
        !xl6BufferInput(buffer,size,&data)) return NULL;
    return crypt(key,nonce,data,counter);
}
xvalue* xl6CryptoChaCha20TextSpanV1(xbytesview key,xbytesview nonce,
    xstrview text,int64_t counter,int64_t size)
{
    if (!xl6BufferSpanInput(key,-1,&key) || !xl6BufferSpanInput(nonce,-1,&nonce) ||
        !xl6TextSelectRaw(&text,size)) return NULL;
    return crypt(key,nonce,(xbytesview){(const uint8*)text.Data,text.Size},counter);
}
xvalue* xl6CryptoChaCha20BytesSpanV1(xbytesview key,xbytesview nonce,
    xbytesview data,int64_t counter,int64_t size)
{
    if (!xl6BufferSpanInput(key,-1,&key) || !xl6BufferSpanInput(nonce,-1,&nonce) ||
        !xl6BufferSpanInput(data,size,&data)) return NULL;
    return crypt(key,nonce,data,counter);
}
xvalue* xl6CryptoChaCha20BufferSpanV1(xbytesview key,xbytesview nonce,
    void* buffer,int64_t counter,int64_t size)
{
    xbytesview data;
    if (!xl6BufferSpanInput(key,-1,&key) || !xl6BufferSpanInput(nonce,-1,&nonce) ||
        !xl6BufferInput(buffer,size,&data)) return NULL;
    return crypt(key,nonce,data,counter);
}
