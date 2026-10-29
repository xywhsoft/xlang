#include "xl6_key_curve_codecs.h"

xstrview xl6CryptoX25519PublicSpkiPemV1(void* raw)
{return xl6KeyObjectToPemV1(raw,false,"PUBLIC KEY",xl6CryptoX25519WriteSpkiSpanV1);}
