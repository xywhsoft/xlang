#include "xl6_crypto_ed25519_internal.h"
#include "xl6_crypto_abi.h"

xvalue* xl6CryptoEd25519SignBufferV1(const xvalue* seed,void* buffer,int64_t size)
{
    xbytesview message;
    return xl6BufferInput(buffer,size,&message)?xl6CryptoEd25519SeedSignSpanV1(seed,message):NULL;
}
