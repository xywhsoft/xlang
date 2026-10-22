#include "xl6_key_object_internal.h"

xvalue* xl6KeyPublicFromSpkiPemV1(xstrview text,XL6_KeyAlgorithmV1 algorithm)
{
    bytes encoded=NULL;size_t size=0;xvalue* value=NULL;
    if (xl6KeyObjectOutcomeV1(xl6KeyPemDecodeV1(text,"PUBLIC KEY",&encoded,&size)))
        value=xl6KeyPublicFromSpkiSpanV1((xbytesview){encoded,size},algorithm);
    xl6KeyFreeV1(encoded,size?size:1);return xl6KeyObjectOwnedValueV1(value);
}
xstrview xl6KeyPublicToSpkiPemV1(const xvalue* value,XL6_KeyAlgorithmV1 algorithm)
{
    xbuffer encoded={0};bool ok=xl6KeyPublicWriteSpkiV1(value,algorithm,&encoded);
    return xl6KeyObjectBufferPemV1(&encoded,ok,"PUBLIC KEY");
}
