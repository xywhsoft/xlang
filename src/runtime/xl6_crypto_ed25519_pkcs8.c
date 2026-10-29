#include "xl6_key_curve_codecs.h"

void* xl6CryptoEd25519FromPkcs8SpanV1(xbytesview input)
{return xl6KeyObjectFromPkcs8V1(&xl6CryptoEd25519ImportSpecV1,input);}
void* xl6CryptoEd25519FromPkcs8V1(const xvalue* value)
{
    xbytesview input;
    return xl6KeyObjectInputV1(value,&input)?xl6CryptoEd25519FromPkcs8SpanV1(input):NULL;
}
bool xl6CryptoEd25519WritePkcs8SpanV1(void* raw,bool includePublic,xbuffer* output)
{return xl6KeyObjectWritePkcs8V1(&xl6CryptoEd25519ExportSpecV1,raw,includePublic,output);}
xvalue* xl6CryptoEd25519ToPkcs8V1(void* raw,bool includePublic)
{
    xbuffer output={0};bool ok=xl6CryptoEd25519WritePkcs8SpanV1(raw,includePublic,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
