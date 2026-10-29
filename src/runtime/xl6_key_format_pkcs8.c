#include "xl6_key_format_internal.h"

/* Attributes are an IMPLICIT SET OF Attribute. Check their ordinary DER
 * schema and lexicographic SET order before discarding metadata. No language
 * callback, attribute interpretation or lifetime escapes this bounded read. */
static bool attributes(const xdervalue* value)
{
    xdercursor list; xbytesview previous={0};
    if (!xrtDerEnter(value,&list)) return false;
    while (!xrtDerDone(&list)) {
        xdervalue attribute,oid,set; xdercursor fields,values;
        if (!xrtDerExpect(&list,XASN1_UNIVERSAL,XASN1_SEQUENCE,true,&attribute) ||
            !xrtDerEnter(&attribute,&fields) ||
            !xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_OBJECT_IDENTIFIER,false,&oid) ||
            !xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_SET,true,&set) ||
            !xl6KeyDoneV1(&fields) || !xrtDerEnter(&set,&values)) return false;
        if (xrtDerDone(&values))
            return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_SCHEMA_V1,"empty key attribute value set");
        if (previous.Size) {
            size_t count=previous.Size<attribute.Raw.Size?previous.Size:attribute.Raw.Size;
            int order=memcmp(previous.Data,attribute.Raw.Data,count);
            if (order>0 || (!order && previous.Size>attribute.Raw.Size))
                return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_SCHEMA_V1,"key attributes are not in DER SET order");
        }
        previous=attribute.Raw;
    }
    return true;
}
bool xl6KeyPkcs8ReadV1(xbytesview input,XL6_KeyAlgorithmV1 algorithm,
    xbytesview* privateKey,xbytesview* publicKey)
{
    xdercursor fields; xdervalue versionValue,octets,value;
    xbytesview secret,publicResult={0}; uint64 version;
    if (!privateKey || !publicKey || privateKey==publicKey)
        return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing independent private/public outputs");
    if (!xl6KeyOpenV1(input,&fields) ||
        !xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_INTEGER,false,&versionValue) ||
        !xrtDerUInt64(&versionValue,&version)) return false;
    if (version>1) return xl6KeyFormatFailV1(XERR_UNSUPPORTED,XL6_KEY_FORMAT_VERSION_V1,"unsupported private key container version");
    if (!xl6KeyReadAlgorithmV1(&fields,algorithm) ||
        !xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_OCTET_STRING,false,&octets) ||
        !xrtDerOctets(&octets,&secret)) return false;
    if (!xrtDerDone(&fields)) {
        if (xrtDerPeek(&fields,&value)!=XDER_VALUE) return false;
        if (xrtDerIs(&value,XASN1_CONTEXT,0,true)) {
            if (xrtDerRead(&fields,&value)!=XDER_VALUE || !attributes(&value)) return false;
        }
    }
    if (!xrtDerDone(&fields)) {
        if (!xrtDerExpect(&fields,XASN1_CONTEXT,1,false,&value)) return false;
        /* [1] is IMPLICIT BIT STRING, including its unused-bits octet. */
        if (version!=1 || !value.Value.Size || value.Value.Data[0]!=0)
            return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_SCHEMA_V1,"invalid private container public key field");
        publicResult=(xbytesview){value.Value.Data+1,value.Value.Size-1};
        if (!xl6KeyPublicShapeV1(algorithm,publicResult)) return false;
    }
    if (!xl6KeyDoneV1(&fields)) return false;
    if ((version==1)!=(publicResult.Data!=NULL))
        return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_VERSION_V1,"private key version and public key presence disagree");
    *privateKey=secret;*publicKey=publicResult;return true;
}
bool xl6KeyPkcs8WriteV1(xbytesview privateKey,xbytesview publicKey,
    XL6_KeyAlgorithmV1 algorithm,xbuffer* output)
{
    if (!output) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing key output buffer");
    if (!xl6KeyInputV1(privateKey)) return false;
    bool withPublic=publicKey.Data!=NULL;
    if ((!withPublic && publicKey.Size) || (withPublic && !xl6KeyPublicShapeV1(algorithm,publicKey)))
        return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"invalid optional public key");
    xbuffer fields={0},bits={0};
    bool ok=xrtDerAppendUInt64(&fields,withPublic?1:0) &&
        xl6KeyWriteAlgorithmV1(&fields,algorithm) && xrtDerAppendOctets(&fields,privateKey);
    if (ok && withPublic)
        ok=xrtBufferAppendByte(&bits,0) && xrtBufferAppend(&bits,publicKey) &&
            xrtDerAppend(&fields,XASN1_CONTEXT,1,false,xrtBufferView(&bits));
    if (ok) ok=xrtDerAppend(output,XASN1_UNIVERSAL,XASN1_SEQUENCE,true,xrtBufferView(&fields));
    xl6KeyBufferFreeV1(&bits);xl6KeyBufferFreeV1(&fields);return ok;
}
