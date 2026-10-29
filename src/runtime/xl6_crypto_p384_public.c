#include "xl6_crypto_nist_internal.h"
#include "xl6_crypto_abi.h"
xvalue* xl6CryptoP384PublicFromPrivateV1(const xvalue* value)
{
    xbytesview key;
    if (!xl6CryptoNistMaterial(value,48,&key)) return NULL;
    void* output=xrtMalloc(97);if (!output) return NULL;
    return xl6CryptoTakeSecret(output,97,xrtP384Public(key.Data,output));
}
