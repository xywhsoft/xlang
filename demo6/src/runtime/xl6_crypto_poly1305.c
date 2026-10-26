#include "xl6_crypto_poly1305_internal.h"
#include "xl6_crypto_abi.h"

/* The key is a complete 32-byte one-time key. Only message data has a prefix
 * limit. No retained state, temporary Buffer, key copy or NUL scan is needed. */
xvalue* xl6CryptoPoly1305TextSpanV1(xbytesview key,xstrview text,int64_t size)
{
    if (!xl6BufferSpanInput(key,-1,&key) || !xl6TextSelectRaw(&text,size)) return NULL;
    return xl6CryptoPoly1305Authenticate(key,(xbytesview){(const uint8*)text.Data,text.Size});
}
xvalue* xl6CryptoPoly1305BytesSpanV1(xbytesview key,xbytesview input,int64_t size)
{
    if (!xl6BufferSpanInput(key,-1,&key) || !xl6BufferSpanInput(input,size,&input)) return NULL;
    return xl6CryptoPoly1305Authenticate(key,input);
}
xvalue* xl6CryptoPoly1305TextV1(const xvalue* keyValue,xstrview text,int64_t size)
{xbytesview key;return xl6CryptoMaterial(keyValue,&key)?xl6CryptoPoly1305TextSpanV1(key,text,size):NULL;}
xvalue* xl6CryptoPoly1305BytesV1(const xvalue* keyValue,const xvalue* value,int64_t size)
{
    xbytesview key,input;
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6BufferBytesInput(value,size,&input)) return NULL;
    return xl6CryptoPoly1305Authenticate(key,input);
}
