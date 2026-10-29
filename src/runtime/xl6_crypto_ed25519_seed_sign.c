#include "xl6_crypto_ed25519_internal.h"
#include "xl6_crypto_abi.h"

xvalue* xl6CryptoEd25519SeedSignSpanV1(const xvalue* seedValue,xbytesview message)
{
    xbytesview seed;
    if (!xl6CryptoEd25519Material(seedValue,XRT_ED25519_SEED_SIZE,&seed)) return NULL;
    void* output=xrtMalloc(XRT_ED25519_SIGNATURE_SIZE);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,XRT_ED25519_SIGNATURE_SIZE,
        xrtEd25519Sign(seed.Data,message.Data,message.Size,output));
}
xvalue* xl6CryptoEd25519SignTextV1(const xvalue* seed,xstrview message,int64_t size)
{
    return xl6TextSelectRaw(&message,size)?xl6CryptoEd25519SeedSignSpanV1(seed,
        (xbytesview){(const uint8*)message.Data,message.Size}):NULL;
}
xvalue* xl6CryptoEd25519SignBytesV1(const xvalue* seed,const xvalue* value,int64_t size)
{
    xbytesview message;
    return xl6BufferBytesInput(value,size,&message)?xl6CryptoEd25519SeedSignSpanV1(seed,message):NULL;
}
