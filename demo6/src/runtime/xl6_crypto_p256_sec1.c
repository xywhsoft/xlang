#include "xl6_key_curve_codecs.h"

void* xl6CryptoP256FromSec1SpanV1(xbytesview input)
{return xl6KeyObjectFromInnerV1(&xl6CryptoP256ImportSpecV1,input,false,(xbytesview){0});}
void* xl6CryptoP256FromSec1V1(const xvalue* value)
{
    xbytesview input;
    return xl6KeyObjectInputV1(value,&input)?xl6CryptoP256FromSec1SpanV1(input):NULL;
}
bool xl6CryptoP256WriteSec1SpanV1(void* raw,bool includePublic,xbuffer* output)
{return xl6KeyObjectWriteInnerV1(&xl6CryptoP256ExportSpecV1,raw,includePublic,output);}
xvalue* xl6CryptoP256ToSec1V1(void* raw,bool includePublic)
{
    xbuffer output={0};bool ok=xl6CryptoP256WriteSec1SpanV1(raw,includePublic,&output);
    return xl6KeyObjectValueV1(&output,ok);
}
