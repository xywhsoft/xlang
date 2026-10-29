#include "xl6_key_curve_codecs.h"
void* xl6CryptoRsaPublicFromPkcs1PemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"RSA PUBLIC KEY",xl6CryptoRsaPublicFromPkcs1SpanV1,xl6CryptoRsaPublicDestroyV1);}
xstrview xl6CryptoRsaPublicToPkcs1PemV1(void* raw)
{return xl6KeyObjectToPemV1(raw,false,"RSA PUBLIC KEY",xl6CryptoRsaPublicWritePkcs1SpanV1);}
