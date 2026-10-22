#include "xl6_key_format_internal.h"

bool xl6KeySpkiReadV1(xbytesview input,XL6_KeyAlgorithmV1 algorithm,xbytesview* publicKey)
{
    xdercursor fields; xdervalue bits; xbytesview result; uint8 unused;
    if (!publicKey) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing public key output");
    if (!xl6KeyOpenV1(input,&fields) || !xl6KeyReadAlgorithmV1(&fields,algorithm) ||
        !xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_BIT_STRING,false,&bits) ||
        !xrtDerBitString(&bits,&result,&unused) || !xl6KeyDoneV1(&fields)) return false;
    if (unused) return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_SCHEMA_V1,"public key BIT STRING is not byte aligned");
    if (!xl6KeyPublicShapeV1(algorithm,result)) return false;
    *publicKey=result;return true;
}
bool xl6KeySpkiWriteV1(xbytesview publicKey,XL6_KeyAlgorithmV1 algorithm,xbuffer* output)
{
    if (!output) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing key output buffer");
    if (!xl6KeyPublicShapeV1(algorithm,publicKey)) return false;
    xbuffer fields={0};
    bool ok=xl6KeyWriteAlgorithmV1(&fields,algorithm) && xrtDerAppendBitString(&fields,publicKey,0) &&
        xrtDerAppend(output,XASN1_UNIVERSAL,XASN1_SEQUENCE,true,xrtBufferView(&fields));
    xl6KeyBufferFreeV1(&fields);return ok;
}
