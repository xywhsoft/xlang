#include "xl6_key_curve_codecs.h"

void* xl6CryptoX448FromPkcs8SpanV1(xbytesview input)
{return xl6KeyObjectFromPkcs8V1(&xl6CryptoX448ImportSpecV1,input);}
void* xl6CryptoX448FromPkcs8V1(const xvalue* value)
{
    xbytesview input;
    return xl6KeyObjectInputV1(value,&input)?xl6CryptoX448FromPkcs8SpanV1(input):NULL;
}
bool xl6CryptoX448WritePkcs8SpanV1(void* raw,bool includePublic,xbuffer* output)
{return xl6KeyObjectWritePkcs8V1(&xl6CryptoX448ExportSpecV1,raw,includePublic,output);}
xvalue* xl6CryptoX448ToPkcs8V1(void* raw,bool includePublic)
{
    xbuffer output={0};bool ok=xl6CryptoX448WritePkcs8SpanV1(raw,includePublic,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
