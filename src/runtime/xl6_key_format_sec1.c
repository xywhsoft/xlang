#include "xl6_key_format_internal.h"

static const XL6_KeySpecV1* ecSpec(XL6_KeyAlgorithmV1 algorithm)
{
    const XL6_KeySpecV1* spec=xl6KeySpecV1Get(algorithm);
    if (spec && spec->ecPoint) return spec;
    xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_ALGORITHM_V1,"SEC1 requires an explicit supported EC curve");return NULL;
}
bool xl6KeySec1ReadV1(xbytesview input,XL6_KeyAlgorithmV1 algorithm,
    bool outerCurve,xbytesview* privateKey,xbytesview* publicKey)
{
    const XL6_KeySpecV1* spec=ecSpec(algorithm);
    xdercursor fields,explicitValue; xdervalue value,inner;
    uint64 version; xbytesview secret,publicResult={0}; bool parameters=false;
    if (!privateKey || !publicKey || privateKey==publicKey)
        return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing independent EC key outputs");
    if (!spec || !xl6KeyOpenV1(input,&fields) ||
        !xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_INTEGER,false,&value) || !xrtDerUInt64(&value,&version)) return false;
    if (version!=1) return xl6KeyFormatFailV1(XERR_UNSUPPORTED,XL6_KEY_FORMAT_VERSION_V1,"unsupported EC private key version");
    if (!xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_OCTET_STRING,false,&value) || !xrtDerOctets(&value,&secret)) return false;
    if (secret.Size!=spec->privateSize)
        return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_MATERIAL_V1,"EC private key has the wrong width");
    if (!xrtDerDone(&fields)) {
        if (xrtDerPeek(&fields,&value)!=XDER_VALUE) return false;
        if (xrtDerIs(&value,XASN1_CONTEXT,0,true)) {
            if (xrtDerRead(&fields,&value)!=XDER_VALUE || !xrtDerEnter(&value,&explicitValue) ||
                !xrtDerExpect(&explicitValue,XASN1_UNIVERSAL,XASN1_OBJECT_IDENTIFIER,false,&inner) ||
                !xl6KeyDoneV1(&explicitValue)) return false;
            if (!xrtDerOidEqual(&inner,spec->parameters.Data,spec->parameters.Size))
                return xl6KeyFormatFailV1(XERR_UNSUPPORTED,XL6_KEY_FORMAT_ALGORITHM_V1,"SEC1 named curve does not match the explicit target");
            parameters=true;
        }
    }
    /* Standalone RFC5915 requires named parameters. PKCS#8 can provide the
     * already authenticated outer curve (e.g. conventional OpenSSL output). */
    if (!parameters && !outerCurve)
        return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_SCHEMA_V1,"standalone EC key requires named curve parameters");
    if (!xrtDerDone(&fields)) {
        uint8 unused;
        if (!xrtDerExpect(&fields,XASN1_CONTEXT,1,true,&value) || !xrtDerEnter(&value,&explicitValue) ||
            !xrtDerExpect(&explicitValue,XASN1_UNIVERSAL,XASN1_BIT_STRING,false,&inner) ||
            !xrtDerBitString(&inner,&publicResult,&unused) || !xl6KeyDoneV1(&explicitValue)) return false;
        if (unused || !xl6KeyPublicShapeV1(algorithm,publicResult))
            return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_SCHEMA_V1,"invalid SEC1 public key field");
    }
    if (!xl6KeyDoneV1(&fields)) return false;
    *privateKey=secret;*publicKey=publicResult;return true;
}
bool xl6KeySec1WriteV1(xbytesview privateKey,xbytesview publicKey,
    XL6_KeyAlgorithmV1 algorithm,xbuffer* output)
{
    const XL6_KeySpecV1* spec=ecSpec(algorithm);
    if (!spec) return false;
    if (!output || !privateKey.Data || privateKey.Size!=spec->privateSize)
        return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_MATERIAL_V1,"invalid EC private key or output");
    bool withPublic=publicKey.Data!=NULL;
    if (!withPublic && publicKey.Size)
        return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_MATERIAL_V1,"missing optional EC public key storage");
    if (withPublic && !xl6KeyPublicShapeV1(algorithm,publicKey)) return false;
    xbuffer fields={0},parameters={0},bits={0};
    bool ok=xrtDerAppendUInt64(&fields,1) && xrtDerAppendOctets(&fields,privateKey) &&
        xrtDerAppend(&parameters,XASN1_UNIVERSAL,XASN1_OBJECT_IDENTIFIER,false,spec->parameters) &&
        xrtDerAppend(&fields,XASN1_CONTEXT,0,true,xrtBufferView(&parameters));
    if (ok && withPublic)
        ok=xrtDerAppendBitString(&bits,publicKey,0) && xrtDerAppend(&fields,XASN1_CONTEXT,1,true,xrtBufferView(&bits));
    if (ok) ok=xrtDerAppend(output,XASN1_UNIVERSAL,XASN1_SEQUENCE,true,xrtBufferView(&fields));
    xl6KeyBufferFreeV1(&bits);xl6KeyBufferFreeV1(&parameters);xl6KeyBufferFreeV1(&fields);return ok;
}
