#include "xl6_crypto_ed25519_internal.h"
#include "xl6_crypto_abi.h"

bool xl6CryptoEd25519VerifyBufferV1(const xvalue* publicKey,void* buffer,const xvalue* signature,int64_t size)
{
    xbytesview message;
    return xl6BufferInput(buffer,size,&message)&&xl6CryptoEd25519VerifySpanV1(publicKey,message,signature,XED25519_PURE,NULL);
}
bool xl6CryptoEd25519VerifyContextBufferV1(const xvalue* publicKey,void* buffer,const xvalue* signature,const xvalue* context,int64_t size)
{
    xbytesview message;
    return xl6BufferInput(buffer,size,&message)&&xl6CryptoEd25519VerifySpanV1(publicKey,message,signature,XED25519_CONTEXT,context);
}
