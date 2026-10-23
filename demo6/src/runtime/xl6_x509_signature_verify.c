#include "xl6_x509_verify_internal.h"
#include "xl6_buffer_internal.h"

int64_t xl6X509SignatureRunSpanV1(void* raw,xbytesview content,const xvalue* signature,void* publicKey,bool strict)
{
    const XL6_X509SignatureInfoV1* scheme=xl6X509SignatureInfoCheckedV1(raw);
    const XL6_X509PublicKeyInfoV1* key=xl6X509PublicKeyInfoCheckedV1(publicKey);xbytesview encoded;
    if(!scheme || !key || !xl6BufferBytesInput(signature,-1,&encoded))return XL6_X509_SIGNATURE_ERROR_V1;
    xerror* previous=xrtTakeError();
    bool ok=xrtX509SignatureVerify(&scheme->value,content,encoded,&key->value);
    return xl6X509SignatureFinishV1(ok,previous,strict);
}
int64_t xl6X509SignatureInfoCheckBytesV1(void* raw,const xvalue* content,const xvalue* signature,void* key,int64_t size)
{xbytesview selected;return xl6BufferBytesInput(content,size,&selected)?xl6X509SignatureRunSpanV1(raw,selected,signature,key,false):XL6_X509_SIGNATURE_ERROR_V1;}
int64_t xl6X509SignatureInfoCheckTextV1(void* raw,xstrview content,const xvalue* signature,void* key,int64_t size)
{return xl6TextSelectRaw(&content,size)?xl6X509SignatureRunSpanV1(raw,(xbytesview){(const uint8*)content.Data,content.Size},signature,key,false):XL6_X509_SIGNATURE_ERROR_V1;}
void xl6X509SignatureInfoVerifyBytesV1(void* raw,const xvalue* content,const xvalue* signature,void* key,int64_t size)
{xbytesview selected;if(xl6BufferBytesInput(content,size,&selected))(void)xl6X509SignatureRunSpanV1(raw,selected,signature,key,true);}
void xl6X509SignatureInfoVerifyTextV1(void* raw,xstrview content,const xvalue* signature,void* key,int64_t size)
{if(xl6TextSelectRaw(&content,size))(void)xl6X509SignatureRunSpanV1(raw,(xbytesview){(const uint8*)content.Data,content.Size},signature,key,true);}
