#include "xl6_crypto_nist_verify_internal.h"
#include "xl6_crypto_abi.h"
bool xl6CryptoP384VerifyHashV1(const xvalue* publicValue,const xvalue* digestValue,const xvalue* signatureValue)
{
    xbytesview key,digest,signature;
    if (!xl6CryptoNistMaterial(publicValue,97,&key) ||
        !xl6CryptoNistDigest(digestValue,&digest) ||
        !xl6CryptoNistMaterial(signatureValue,96,&signature)) return false;
    xerror* previous=xrtTakeError();
    bool ok=xrtEcdsaP384Verify(digest.Data,digest.Size,signature.Data,key.Data);
    return xl6CryptoNistVerifyFinish(ok,previous);
}
