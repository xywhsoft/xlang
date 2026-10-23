#include "xl6_x509_records_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509ExtensionsDestroyV1(void* raw){xl6X509FreeV1(raw);}
XL6_X509ExtensionsV1* xl6X509ExtensionsCopyV1(xbytesview input,int64_t limit)
{
    if((input.Size && !input.Data) || input.Size>INT64_MAX){xl6X509FailV1(XERR_ARGUMENT,"invalid Extensions storage");return NULL;}
    if(!xl6X509LimitV1(0,limit,"invalid extension budget"))return NULL;
    xx509extcursor cursor;xx509ext item;size_t count=0;
    if(!xl6X509OutcomeV1(xrtX509ExtensionListInit(input,&cursor)))return NULL;
    for(;;) {
        xx509result status=xrtX509ExtensionRead(&cursor,&item);
        if(status==X509_DONE)break;
        if(!xl6X509OutcomeV1(status==X509_VALUE))return NULL;
        if(count==SIZE_MAX){xl6X509FailV1(XERR_RANGE,"extension count overflow");return NULL;}
        if(!xl6X509LimitV1(++count,limit,"extension budget exceeded"))return NULL;
    }
    void* items=NULL;xbytesview owned;
    XL6_X509ExtensionsV1* self=xl6X509RecordAllocV1(sizeof(*self),count,sizeof(xx509ext),_Alignof(xx509ext),input,&items,&owned);
    if(!self)return NULL;
    self->count=count;self->items=items;self->raw=owned;
    bool ok=xrtX509ExtensionListInit(owned,&cursor);
    for(size_t i=0;ok && i<count;++i)ok=xrtX509ExtensionRead(&cursor,&self->items[i])==X509_VALUE;
    if(ok)ok=xrtX509ExtensionRead(&cursor,&item)==X509_DONE;
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509ExtensionsDestroyV1(self);return NULL;
}
void* xl6X509ExtensionsFromDerV1(const xvalue* value,int64_t bytes,int64_t count)
{
    xbytesview input;
    return xl6X509OutcomeV1(xrtValueGetBytes(value,&input)) && xl6X509LimitV1(input.Size,bytes,"Extensions DER exceeds input budget")?
        xl6X509ExtensionsCopyV1(input,count):NULL;
}
xvalue* xl6X509ExtensionsToDerV1(void* raw)
{const XL6_X509ExtensionsV1* self=xl6X509ExtensionsCheckedV1(raw);return self?xl6X509BytesV1(self->raw):NULL;}
uint64_t xl6X509ExtensionsCountV1(void* raw)
{const XL6_X509ExtensionsV1* self=xl6X509ExtensionsCheckedV1(raw);return self?(uint64_t)self->count:0;}
void* xl6X509ExtensionsAtV1(void* raw,int64_t index)
{
    const XL6_X509ExtensionsV1* self=xl6X509ExtensionsCheckedV1(raw);size_t selected;
    return self && xl6X509IndexV1(index,self->count,&selected)?xl6X509ExtensionCopyV1(&self->items[selected]):NULL;
}
static void* find(const XL6_X509ExtensionsV1* self,xbytesview oid)
{
    for(size_t i=0;i<self->count;++i) {
        const xx509ext* item=&self->items[i];
        if(item->Oid.Size==oid.Size && !memcmp(item->Oid.Data,oid.Data,oid.Size))return xl6X509ExtensionCopyV1(item);
    }
    return NULL;
}
void* xl6X509ExtensionsFindOidV1(void* raw,const xvalue* value)
{
    const XL6_X509ExtensionsV1* self=xl6X509ExtensionsCheckedV1(raw);xbytesview oid;
    return self && xl6X509OidInputV1(value,&oid)?find(self,oid):NULL;
}
void* xl6X509ExtensionsFindV1(void* raw,xstrview text)
{
    const XL6_X509ExtensionsV1* self=xl6X509ExtensionsCheckedV1(raw);if(!self)return NULL;
    xbuffer oid={0};void* result=xl6X509OidEncodeV1(text,&oid)?find(self,xrtBufferView(&oid)):NULL;
    xl6X509BufferFreeV1(&oid);
    if(xrtGetError()){xl6X509ExtensionDestroyV1(result);return NULL;}
    return result;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509ExtensionsHandlePolicyV1Get,xl6X509ExtensionsDestroyV1)
