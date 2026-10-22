#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
bool xl6CryptoP256ValidV1(const xvalue* pointValue)
{
    xbytesview point;
    return xl6CryptoNistMaterial(pointValue,65,&point)&&xrtP256Valid(point.Data);
}
xvalue* xl6CryptoP256MultiplyV1(const xvalue* scalarValue,const xvalue* pointValue)
{
    xbytesview scalar,point;
    if (!xl6CryptoNistMaterial(scalarValue,32,&scalar) ||
        !xl6CryptoNistMaterial(pointValue,65,&point)) return NULL;
    void* output=xrtMalloc(65);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,65,xrtP256Multiply(scalar.Data,point.Data,output));
}
xvalue* xl6CryptoP256AddV1(const xvalue* leftValue,const xvalue* rightValue)
{
    xbytesview left,right;
    if (!xl6CryptoNistMaterial(leftValue,65,&left) ||
        !xl6CryptoNistMaterial(rightValue,65,&right)) return NULL;
    void* output=xrtMalloc(65);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,65,xrtP256Add(left.Data,right.Data,output));
}
