#include "xl6_key_object_internal.h"

xvalue* xl6KeyPublicFromSpkiSpanV1(xbytesview input,XL6_KeyAlgorithmV1 algorithm)
{
    xbytesview publicKey;
    if (!xl6KeyObjectOutcomeV1(xl6KeySpkiReadV1(input,algorithm,&publicKey))) return NULL;
    return xl6KeyObjectOwnedValueV1(xrtValueBytes(publicKey));
}
xvalue* xl6KeyPublicFromSpkiV1(const xvalue* input,XL6_KeyAlgorithmV1 algorithm)
{
    xbytesview encoded;
    return xl6KeyObjectInputV1(input,&encoded)?xl6KeyPublicFromSpkiSpanV1(encoded,algorithm):NULL;
}
bool xl6KeyPublicWriteSpkiV1(const xvalue* value,XL6_KeyAlgorithmV1 algorithm,xbuffer* output)
{
    xbytesview publicKey;
    return xl6KeyObjectInputV1(value,&publicKey) && xl6KeyObjectOutcomeV1(xl6KeySpkiWriteV1(publicKey,algorithm,output));
}
xvalue* xl6KeyPublicToSpkiV1(const xvalue* value,XL6_KeyAlgorithmV1 algorithm)
{
    xbuffer encoded={0};bool ok=xl6KeyPublicWriteSpkiV1(value,algorithm,&encoded);
    return xl6KeyObjectValueV1(&encoded,ok);
}
