#include "xl6_key_curve_codecs.h"
void* xl6CryptoRsaPublicFromSpkiPemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"PUBLIC KEY",xl6CryptoRsaPublicFromSpkiSpanV1,xl6CryptoRsaPublicDestroyV1);}
xstrview xl6CryptoRsaPublicToSpkiPemV1(void* raw)
{return xl6KeyObjectToPemV1(raw,false,"PUBLIC KEY",xl6CryptoRsaPublicWriteSpkiSpanV1);}
