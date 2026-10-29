#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
bool xl6CryptoP384ValidV1(const xvalue* pointValue)
{
    xbytesview point;
    return xl6CryptoNistMaterial(pointValue,97,&point)&&xrtP384Valid(point.Data);
}
xvalue* xl6CryptoP384MultiplyV1(const xvalue* scalarValue,const xvalue* pointValue)
{
    xbytesview scalar,point;
    if (!xl6CryptoNistMaterial(scalarValue,48,&scalar) ||
        !xl6CryptoNistMaterial(pointValue,97,&point)) return NULL;
    void* output=xrtMalloc(97);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,97,xrtP384Multiply(scalar.Data,point.Data,output));
}
xvalue* xl6CryptoP384AddV1(const xvalue* leftValue,const xvalue* rightValue)
{
    xbytesview left,right;
    if (!xl6CryptoNistMaterial(leftValue,97,&left) ||
        !xl6CryptoNistMaterial(rightValue,97,&right)) return NULL;
    void* output=xrtMalloc(97);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,97,xrtP384Add(left.Data,right.Data,output));
}
