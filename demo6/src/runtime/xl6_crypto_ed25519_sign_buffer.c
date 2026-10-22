#include "xl6_crypto_ed25519_internal.h"
#include "xl6_crypto_abi.h"

xvalue* xl6CryptoEd25519KeySignBufferV1(void* self,void* buffer,int64_t size)
{
    xbytesview message;
    return xl6BufferInput(buffer,size,&message)?xl6CryptoEd25519KeySignSpanV1(self,message,XED25519_PURE,NULL):NULL;
}
xvalue* xl6CryptoEd25519KeySignContextBufferV1(void* self,void* buffer,const xvalue* context,int64_t size)
{
    xbytesview message;
    return xl6BufferInput(buffer,size,&message)?xl6CryptoEd25519KeySignSpanV1(self,message,XED25519_CONTEXT,context):NULL;
}
