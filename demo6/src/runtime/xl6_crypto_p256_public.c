#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoP256PublicFromPrivateV1(const xvalue* value)
{
    xbytesview key;
    if (!xl6CryptoNistMaterial(value,32,&key)) return NULL;
    void* output=xrtMalloc(65);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,65,xrtP256Public(key.Data,output));
}
