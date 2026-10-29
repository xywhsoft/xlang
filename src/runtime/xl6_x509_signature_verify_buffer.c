#include "xl6_x509_verify_internal.h"
#include "xl6_buffer_internal.h"
int64_t xl6X509SignatureInfoCheckBufferV1(void* raw,void* content,const xvalue* signature,void* key,int64_t size)
{xbytesview selected;return xl6BufferInput(content,size,&selected)?xl6X509SignatureRunSpanV1(raw,selected,signature,key,false):XL6_X509_SIGNATURE_ERROR_V1;}
void xl6X509SignatureInfoVerifyBufferV1(void* raw,void* content,const xvalue* signature,void* key,int64_t size)
{xbytesview selected;if(xl6BufferInput(content,size,&selected))(void)xl6X509SignatureRunSpanV1(raw,selected,signature,key,true);}
