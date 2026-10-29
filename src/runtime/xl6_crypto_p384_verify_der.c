#include "xl6_crypto_nist_verify_internal.h"
#include "xl6_crypto_abi.h"
bool xl6CryptoP384VerifyHashDerV1(const xvalue* publicValue,const xvalue* digestValue,const xvalue* signatureValue)
{
    xbytesview key,digest,signature;
    if (!xl6CryptoNistMaterial(publicValue,97,&key) ||
        !xl6CryptoNistDigest(digestValue,&digest) ||
        !xl6CryptoMaterial(signatureValue,&signature)) return false;
    xerror* previous=xrtTakeError();
    bool ok=xrtEcdsaP384VerifyDer(digest.Data,digest.Size,signature.Data,signature.Size,key.Data);
    return xl6CryptoNistVerifyFinish(ok,previous);
}
