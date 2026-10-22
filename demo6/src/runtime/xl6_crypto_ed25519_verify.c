#include "xl6_crypto_ed25519_verify_internal.h"
#include "xl6_crypto_abi.h"

bool xl6CryptoEd25519VerifySpanV1(const xvalue* publicValue,xbytesview message,const xvalue* signatureValue,
    xed25519mode mode,const xvalue* contextValue)
{
    xbytesview publicKey,signature,context={0};
    if (!xl6CryptoEd25519Material(publicValue,XRT_ED25519_PUBLIC_SIZE,&publicKey) ||
        !xl6CryptoEd25519Material(signatureValue,XRT_ED25519_SIGNATURE_SIZE,&signature)) return false;
    if (mode!=XED25519_PURE && !xl6CryptoEd25519Context(contextValue,&context)) return false;
    if (mode==XED25519_PREHASH && message.Size!=XRT_ED25519_PREHASH_SIZE) {xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xerror* previous=xrtTakeError();
    bool ok=xrtEd25519VerifyMode(publicKey.Data,mode,context.Data,context.Size,message.Data,message.Size,signature.Data);
    return xl6CryptoEd25519VerifyFinish(ok,previous);
}
bool xl6CryptoEd25519VerifyTextV1(const xvalue* publicKey,xstrview message,const xvalue* signature,int64_t size)
{
    return xl6TextSelectRaw(&message,size)&&xl6CryptoEd25519VerifySpanV1(publicKey,
        (xbytesview){(const uint8*)message.Data,message.Size},signature,XED25519_PURE,NULL);
}
bool xl6CryptoEd25519VerifyBytesV1(const xvalue* publicKey,const xvalue* value,const xvalue* signature,int64_t size)
{
    xbytesview message;
    return xl6BufferBytesInput(value,size,&message)&&xl6CryptoEd25519VerifySpanV1(publicKey,message,signature,XED25519_PURE,NULL);
}
bool xl6CryptoEd25519VerifyContextTextV1(const xvalue* publicKey,xstrview message,const xvalue* signature,const xvalue* context,int64_t size)
{
    return xl6TextSelectRaw(&message,size)&&xl6CryptoEd25519VerifySpanV1(publicKey,
        (xbytesview){(const uint8*)message.Data,message.Size},signature,XED25519_CONTEXT,context);
}
bool xl6CryptoEd25519VerifyContextBytesV1(const xvalue* publicKey,const xvalue* value,const xvalue* signature,const xvalue* context,int64_t size)
{
    xbytesview message;
    return xl6BufferBytesInput(value,size,&message)&&xl6CryptoEd25519VerifySpanV1(publicKey,message,signature,XED25519_CONTEXT,context);
}
bool xl6CryptoEd25519VerifyPrehashV1(const xvalue* publicKey,const xvalue* value,const xvalue* signature,const xvalue* context)
{
    xbytesview digest;
    return xl6CryptoEd25519Material(value,XRT_ED25519_PREHASH_SIZE,&digest)&&
        xl6CryptoEd25519VerifySpanV1(publicKey,digest,signature,XED25519_PREHASH,context);
}
