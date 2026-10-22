#include "xl6_key_format_internal.h"

static const XL6_KeySpecV1* specFor(XL6_KeyAlgorithmV1 algorithm)
{
    const XL6_KeySpecV1* spec=xl6KeySpecV1Get(algorithm);
    if (spec && !spec->ecPoint && !spec->nullParameters) return spec;
    xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_ALGORITHM_V1,"CurvePrivateKey requires an RFC8410 algorithm");return NULL;
}
bool xl6KeyCurvePrivateReadV1(xbytesview input,XL6_KeyAlgorithmV1 algorithm,xbytesview* privateKey)
{
    const XL6_KeySpecV1* spec=specFor(algorithm); xdercursor cursor; xdervalue value; xbytesview result;
    if (!privateKey) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing CurvePrivateKey output");
    if (!spec || !xl6KeyInputV1(input) || !xrtDerValidate(input.Data,input.Size) ||
        !xrtDerInit(&cursor,input.Data,input.Size) ||
        !xrtDerExpect(&cursor,XASN1_UNIVERSAL,XASN1_OCTET_STRING,false,&value) ||
        !xrtDerOctets(&value,&result) || !xl6KeyDoneV1(&cursor)) return false;
    if (result.Size!=spec->privateSize)
        return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_MATERIAL_V1,"CurvePrivateKey has the wrong width");
    *privateKey=result;return true;
}
bool xl6KeyCurvePrivateWriteV1(xbytesview privateKey,XL6_KeyAlgorithmV1 algorithm,xbuffer* output)
{
    const XL6_KeySpecV1* spec=specFor(algorithm);
    if (!spec) return false;
    if (!output || !privateKey.Data || privateKey.Size!=spec->privateSize)
        return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_MATERIAL_V1,"invalid CurvePrivateKey or output");
    return xrtDerAppendOctets(output,privateKey);
}
