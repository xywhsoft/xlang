#include "xl6_key_curve_codecs.h"

void* xl6CryptoP256FromSec1PemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"EC PRIVATE KEY",xl6CryptoP256FromSec1SpanV1,xl6CryptoP256DestroyV1);}
xstrview xl6CryptoP256ToSec1PemV1(void* raw,bool includePublic)
{return xl6KeyObjectToPemV1(raw,includePublic,"EC PRIVATE KEY",xl6CryptoP256WriteSec1SpanV1);}
