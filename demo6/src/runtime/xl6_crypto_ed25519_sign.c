#include "xl6_crypto_ed25519_internal.h"
#include "xl6_crypto_abi.h"

xvalue* xl6CryptoEd25519KeySignSpanV1(void* raw,xbytesview message,xed25519mode mode,const xvalue* contextValue)
{
    const XL6_CryptoEd25519KeyV1* self=raw;xbytesview context={0};
    if (!self) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if (mode!=XED25519_PURE && !xl6CryptoEd25519Context(contextValue,&context)) return NULL;
    if (mode==XED25519_PREHASH && message.Size!=XRT_ED25519_PREHASH_SIZE) {xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    void* output=xrtMalloc(XRT_ED25519_SIGNATURE_SIZE);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,XRT_ED25519_SIGNATURE_SIZE,
        xrtEd25519SignMode(&self->expanded,mode,context.Data,context.Size,message.Data,message.Size,output));
}
xvalue* xl6CryptoEd25519KeySignTextV1(void* self,xstrview message,int64_t size)
{
    return xl6TextSelectRaw(&message,size)?xl6CryptoEd25519KeySignSpanV1(self,
        (xbytesview){(const uint8*)message.Data,message.Size},XED25519_PURE,NULL):NULL;
}
xvalue* xl6CryptoEd25519KeySignBytesV1(void* self,const xvalue* value,int64_t size)
{
    xbytesview message;
    return xl6BufferBytesInput(value,size,&message)?xl6CryptoEd25519KeySignSpanV1(self,message,XED25519_PURE,NULL):NULL;
}
xvalue* xl6CryptoEd25519KeySignContextTextV1(void* self,xstrview message,const xvalue* context,int64_t size)
{
    return xl6TextSelectRaw(&message,size)?xl6CryptoEd25519KeySignSpanV1(self,
        (xbytesview){(const uint8*)message.Data,message.Size},XED25519_CONTEXT,context):NULL;
}
xvalue* xl6CryptoEd25519KeySignContextBytesV1(void* self,const xvalue* value,const xvalue* context,int64_t size)
{
    xbytesview message;
    return xl6BufferBytesInput(value,size,&message)?xl6CryptoEd25519KeySignSpanV1(self,message,XED25519_CONTEXT,context):NULL;
}
xvalue* xl6CryptoEd25519KeySignPrehashV1(void* self,const xvalue* value,const xvalue* context)
{
    xbytesview digest;
    return xl6CryptoEd25519Material(value,XRT_ED25519_PREHASH_SIZE,&digest)?
        xl6CryptoEd25519KeySignSpanV1(self,digest,XED25519_PREHASH,context):NULL;
}
