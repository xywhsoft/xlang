#include "xl6_key_format_internal.h"

/* ASN.1 content octets, not DER TLVs and not C strings. RFC 5480/8410/8017. */
static const uint8 rsa[]={0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01};
static const uint8 ec[]={0x2a,0x86,0x48,0xce,0x3d,0x02,0x01};
static const uint8 p256[]={0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07};
static const uint8 p384[]={0x2b,0x81,0x04,0x00,0x22};
static const uint8 ed25519[]={0x2b,0x65,0x70};
static const uint8 x25519[]={0x2b,0x65,0x6e};
static const uint8 x448[]={0x2b,0x65,0x6f};
#define SPAN(a) {a,sizeof(a)}
static const XL6_KeySpecV1 specs[]={
    {SPAN(rsa),{0},0,0,true,false},
    {SPAN(ec),SPAN(p256),32,65,false,true},
    {SPAN(ec),SPAN(p384),48,97,false,true},
    {SPAN(ed25519),{0},32,32,false,false},
    {SPAN(x25519),{0},32,32,false,false},
    {SPAN(x448),{0},56,56,false,false}
};
#undef SPAN

bool xl6KeyFormatFailV1(xerrkind kind,XL6_KeyFormatErrorV1 code,const char* message)
{
    if (!xrtGetError()) {
        xerror* error=xrtErrorCreate(kind,"xlang.key",(int32)code,message);
        if (error) xrtSetErrorTake(error);
        else if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    }
    return false;
}
const XL6_KeySpecV1* xl6KeySpecV1Get(XL6_KeyAlgorithmV1 algorithm)
{
    if (algorithm<XL6_KEY_RSA_V1 || algorithm>XL6_KEY_X448_V1) {
        xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_ALGORITHM_V1,"unknown key algorithm");
        return NULL;
    }
    return &specs[(size_t)algorithm-1];
}
bool xl6KeyInputV1(xbytesview input)
{
    if (input.Size && !input.Data)
        return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing key input storage");
    if (input.Size>XL6_KEY_DER_LIMIT_V1)
        return xl6KeyFormatFailV1(XERR_RANGE,XL6_KEY_FORMAT_LIMIT_V1,"key DER exceeds input budget");
    return true;
}
bool xl6KeyDoneV1(const xdercursor* cursor)
{
    return xrtDerDone(cursor) || xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_SCHEMA_V1,"unexpected trailing key fields");
}
bool xl6KeyOpenV1(xbytesview input,xdercursor* fields)
{
    xdercursor cursor; xdervalue sequence;
    return xl6KeyInputV1(input) && xrtDerValidate(input.Data,input.Size) &&
        xrtDerInit(&cursor,input.Data,input.Size) &&
        xrtDerExpect(&cursor,XASN1_UNIVERSAL,XASN1_SEQUENCE,true,&sequence) &&
        xl6KeyDoneV1(&cursor) && xrtDerEnter(&sequence,fields);
}
void xl6KeyFreeV1(void* data,size_t size)
{
    xerror* prior=xrtTakeError();
    if (data) xrtSecureZero(data,size);
    xrtFree(data);
    /* A prior operation wins, but an otherwise new cleanup failure must not
     * disappear. Native adapters' checked scopes can then report it. */
    if (prior) {xrtClearError();xrtSetErrorTake(prior);}
}
void xl6KeyBufferFreeV1(xbuffer* buffer)
{
    xl6KeyFreeV1(buffer->Data,buffer->Capacity);
    memset(buffer,0,sizeof(*buffer));
}
bool xl6KeyReadAlgorithmV1(xdercursor* cursor,XL6_KeyAlgorithmV1 algorithm)
{
    const XL6_KeySpecV1* spec=xl6KeySpecV1Get(algorithm);
    xdervalue sequence,oid,parameters; xdercursor fields;
    if (!spec || !xrtDerExpect(cursor,XASN1_UNIVERSAL,XASN1_SEQUENCE,true,&sequence) ||
        !xrtDerEnter(&sequence,&fields) ||
        !xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_OBJECT_IDENTIFIER,false,&oid)) return false;
    if (!xrtDerOidEqual(&oid,spec->algorithm.Data,spec->algorithm.Size))
        return xl6KeyFormatFailV1(XERR_UNSUPPORTED,XL6_KEY_FORMAT_ALGORITHM_V1,"key algorithm does not match the explicit target");
    if (spec->parameters.Size) {
        if (!xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_OBJECT_IDENTIFIER,false,&parameters)) return false;
        if (!xrtDerOidEqual(&parameters,spec->parameters.Data,spec->parameters.Size))
            return xl6KeyFormatFailV1(XERR_UNSUPPORTED,XL6_KEY_FORMAT_ALGORITHM_V1,"key named curve does not match the explicit target");
    } else if (spec->nullParameters && !xrtDerDone(&fields)) {
        if (!xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_NULL,false,&parameters)) return false;
    }
    /* RFC8410 parameters must be ABSENT. RSA accepts NULL or absence, but
     * its writer always emits NULL. Algorithm restrictions are never erased. */
    return xl6KeyDoneV1(&fields);
}
bool xl6KeyWriteAlgorithmV1(xbuffer* output,XL6_KeyAlgorithmV1 algorithm)
{
    const XL6_KeySpecV1* spec=xl6KeySpecV1Get(algorithm); xbuffer content={0};
    bool ok=spec && xrtDerAppend(&content,XASN1_UNIVERSAL,XASN1_OBJECT_IDENTIFIER,false,spec->algorithm);
    if (ok && spec->parameters.Size)
        ok=xrtDerAppend(&content,XASN1_UNIVERSAL,XASN1_OBJECT_IDENTIFIER,false,spec->parameters);
    else if (ok && spec->nullParameters) ok=xrtDerAppendNull(&content);
    if (ok) ok=xrtDerAppend(output,XASN1_UNIVERSAL,XASN1_SEQUENCE,true,xrtBufferView(&content));
    xl6KeyBufferFreeV1(&content);return ok;
}
bool xl6KeyPublicShapeV1(XL6_KeyAlgorithmV1 algorithm,xbytesview publicKey)
{
    const XL6_KeySpecV1* spec=xl6KeySpecV1Get(algorithm);
    if (!spec || !xl6KeyInputV1(publicKey)) return false;
    if (spec->publicSize && (publicKey.Size!=spec->publicSize ||
            (spec->ecPoint && publicKey.Data[0]!=4)))
        return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_MATERIAL_V1,"unexpected public key representation");
    return true; /* Wire shape, not mathematical key validation. */
}
