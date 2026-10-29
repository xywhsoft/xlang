#include "xl6_key_curve_codecs.h"

void* xl6CryptoP384FromSec1PemV1(xstrview text)
{return xl6KeyObjectFromPemV1(text,"EC PRIVATE KEY",xl6CryptoP384FromSec1SpanV1,xl6CryptoP384DestroyV1);}
xstrview xl6CryptoP384ToSec1PemV1(void* raw,bool includePublic)
{return xl6KeyObjectToPemV1(raw,includePublic,"EC PRIVATE KEY",xl6CryptoP384WriteSec1SpanV1);}
