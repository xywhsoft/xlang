#include "xl6_key_format_internal.h"

static bool read(xbytesview input,bool privateKey,xbytesview* parts)
{
    xdercursor fields; xdervalue value; uint64 version;
    xbytesview result[8]={{0}};
    if (!parts) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing RSA components output");
    if (!xl6KeyOpenV1(input,&fields)) return false;
    if (privateKey) {
        if (!xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_INTEGER,false,&value) || !xrtDerUInt64(&value,&version)) return false;
        if (version!=0) return xl6KeyFormatFailV1(XERR_UNSUPPORTED,XL6_KEY_FORMAT_VERSION_V1,"only two-prime RSA keys are supported");
    }
    size_t count=privateKey?8:2;
    for (size_t i=0;i<count;++i) {
        if (!xrtDerExpect(&fields,XASN1_UNIVERSAL,XASN1_INTEGER,false,&value) || !xrtDerUnsigned(&value,&result[i])) return false;
        if (!result[i].Size || result[i].Size>XRT_RSA_MAX_MODULUS_SIZE || result[i].Data[0]==0)
            return xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_MATERIAL_V1,"RSA component is zero or outside the native width limit");
    }
    if (!xl6KeyDoneV1(&fields)) return false;
    memcpy(parts,result,count*sizeof(*parts));return true;
}
static bool write(const xbytesview* parts,bool privateKey,xbuffer* output)
{
    if (!parts || !output) return xl6KeyFormatFailV1(XERR_ARGUMENT,XL6_KEY_FORMAT_SCHEMA_V1,"missing RSA components or output");
    xbuffer fields={0}; uint8 integer[XRT_RSA_MAX_MODULUS_SIZE+1];
    bool ok=!privateKey || xrtDerAppendUInt64(&fields,0);
    size_t count=privateKey?8:2;
    for (size_t i=0;ok && i<count;++i) {
        xbytesview part=parts[i];
        if (!part.Size || !part.Data || part.Size>XRT_RSA_MAX_MODULUS_SIZE) {
            ok=xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_MATERIAL_V1,"complete RSA components are required for PKCS#1 export");break;
        }
        while (part.Size>1 && !part.Data[0]) {++part.Data;--part.Size;}
        if (!part.Data[0]) {ok=xl6KeyFormatFailV1(XERR_VALUE,XL6_KEY_FORMAT_MATERIAL_V1,"RSA component cannot be zero");break;}
        size_t padding=(part.Data[0]&0x80)?1:0;
        if (padding) integer[0]=0;
        memcpy(integer+padding,part.Data,part.Size);
        ok=xrtDerAppend(&fields,XASN1_UNIVERSAL,XASN1_INTEGER,false,(xbytesview){integer,part.Size+padding});
        xrtSecureZero(integer,sizeof(integer));
    }
    if (ok) ok=xrtDerAppend(output,XASN1_UNIVERSAL,XASN1_SEQUENCE,true,xrtBufferView(&fields));
    xrtSecureZero(integer,sizeof(integer));xl6KeyBufferFreeV1(&fields);return ok;
}
bool xl6KeyRsaPublicReadV1(xbytesview input,xbytesview parts[2])
{return read(input,false,parts);}
bool xl6KeyRsaPrivateReadV1(xbytesview input,xbytesview parts[8])
{return read(input,true,parts);}
bool xl6KeyRsaPublicWriteV1(const xbytesview parts[2],xbuffer* output)
{return write(parts,false,output);}
bool xl6KeyRsaPrivateWriteV1(const xbytesview parts[8],xbuffer* output)
{return write(parts,true,output);}
