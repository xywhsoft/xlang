#include "xl6_key_object_internal.h"
#include "xl6_key_abi.h"

xvalue* xl6CryptoP384PublicFromSpkiV1(const xvalue* input)
{return xl6KeyPublicFromSpkiV1(input,XL6_KEY_P384_V1);}
xvalue* xl6CryptoP384PublicToSpkiV1(const xvalue* input)
{return xl6KeyPublicToSpkiV1(input,XL6_KEY_P384_V1);}
