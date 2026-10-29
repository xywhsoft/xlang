#include "xl6_key_object_internal.h"
#include "xl6_key_abi.h"

xvalue* xl6CryptoP256PublicFromSpkiPemV1(xstrview text)
{return xl6KeyPublicFromSpkiPemV1(text,XL6_KEY_P256_V1);}
xstrview xl6CryptoP256PublicToSpkiPemV1(const xvalue* input)
{return xl6KeyPublicToSpkiPemV1(input,XL6_KEY_P256_V1);}
