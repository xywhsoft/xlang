#include "xl6_key_object_internal.h"

void* xl6KeyObjectFromPemV1(xstrview text,const char* label,void* (*read)(xbytesview),void (*destroy)(void*))
{
    bytes encoded=NULL;size_t size=0;void* object=NULL;
    if (xl6KeyObjectOutcomeV1(xl6KeyPemDecodeV1(text,label,&encoded,&size))) object=read((xbytesview){encoded,size});
    xl6KeyFreeV1(encoded,size?size:1);
    if (!xl6KeyObjectOutcomeV1(object!=NULL)) {destroy(object);object=NULL;}
    return object;
}
xstrview xl6KeyObjectBufferPemV1(xbuffer* encoded,bool ok,const char* label)
{
    str text=NULL;size_t size=0;
    if (xl6KeyObjectOutcomeV1(ok)) ok=xl6KeyPemEncodeV1(xrtBufferView(encoded),label,&text,&size);
    xl6KeyBufferFreeV1(encoded);
    if (!xl6KeyObjectOutcomeV1(ok)) {xl6KeyFreeV1(text,text?size+1:0);return xl6TextFailure;}
    return xl6TextOwn(text,size);
}
xstrview xl6KeyObjectToPemV1(void* object,bool withPublic,const char* label,
    bool (*write)(void*,bool,xbuffer*))
{
    xbuffer encoded={0};bool ok=write(object,withPublic,&encoded);
    return xl6KeyObjectBufferPemV1(&encoded,ok,label);
}
