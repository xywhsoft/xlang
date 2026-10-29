#include "xl6_crypto_poly1305_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoPoly1305BufferSpanV1(xbytesview key,void* buffer,int64_t size)
{
    xbytesview input;
    if (!xl6BufferSpanInput(key,-1,&key) || !xl6BufferInput(buffer,size,&input)) return NULL;
    return xl6CryptoPoly1305Authenticate(key,input);
}
xvalue* xl6CryptoPoly1305BufferV1(const xvalue* keyValue,void* buffer,int64_t size)
{
    xbytesview key,input;
    if (!xl6CryptoMaterial(keyValue,&key) || !xl6BufferInput(buffer,size,&input)) return NULL;
    return xl6CryptoPoly1305Authenticate(key,input);
}
