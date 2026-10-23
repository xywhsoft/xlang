#include "xl6_x509_records_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509NameDestroyV1(void* raw){xl6X509FreeV1(raw);}
XL6_X509NameV1* xl6X509NameCopyV1(xbytesview input,int64_t limit)
{
    if(!xl6X509SpanV1(input) || !xl6X509LimitV1(0,limit,"invalid Name attribute budget"))return NULL;
    xx509namecursor cursor;xx509nameattr item;size_t count=0;
    if(!xl6X509OutcomeV1(xrtX509NameInit(input,&cursor)))return NULL;
    for(;;) {
        xx509result status=xrtX509NameRead(&cursor,&item);
        if(status==X509_DONE)break;
        if(!xl6X509OutcomeV1(status==X509_VALUE))return NULL;
        if(count==SIZE_MAX){xl6X509FailV1(XERR_RANGE,"Name attribute count overflow");return NULL;}
        if(!xl6X509LimitV1(++count,limit,"Name attribute budget exceeded"))return NULL;
    }
    void* items=NULL;xbytesview owned;
    XL6_X509NameV1* self=xl6X509RecordAllocV1(sizeof(*self),count,sizeof(xx509nameattr),_Alignof(xx509nameattr),input,&items,&owned);
    if(!self)return NULL;
    self->count=count;self->items=items;self->raw=owned;
    bool ok=xrtX509NameInit(owned,&cursor);
    for(size_t i=0;ok && i<count;++i)ok=xrtX509NameRead(&cursor,&self->items[i])==X509_VALUE;
    if(ok)ok=xrtX509NameRead(&cursor,&item)==X509_DONE;
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509NameDestroyV1(self);return NULL;
}
void* xl6X509NameFromDerV1(const xvalue* value,int64_t bytes,int64_t attributes)
{
    xbytesview input;
    return xl6X509InputV1(value,&input) && xl6X509LimitV1(input.Size,bytes,"Name DER exceeds input budget")?
        xl6X509NameCopyV1(input,attributes):NULL;
}
xvalue* xl6X509NameToDerV1(void* raw)
{const XL6_X509NameV1* self=xl6X509NameCheckedV1(raw);return self?xl6X509BytesV1(self->raw):NULL;}
uint64_t xl6X509NameCountV1(void* raw)
{const XL6_X509NameV1* self=xl6X509NameCheckedV1(raw);return self?(uint64_t)self->count:0;}
void* xl6X509NameAtV1(void* raw,int64_t index)
{
    const XL6_X509NameV1* self=xl6X509NameCheckedV1(raw);size_t selected;
    return self && xl6X509IndexV1(index,self->count,&selected)?xl6X509AttributeCopyV1(&self->items[selected]):NULL;
}
/* The parsed index preserves DER order and first-match semantics. A normal
 * miss does not allocate/discard a NotFound exception or reparse the Name. */
static void* find(const XL6_X509NameV1* self,xbytesview oid)
{
    for(size_t i=0;i<self->count;++i) {
        const xx509nameattr* item=&self->items[i];
        if(item->Oid.Size==oid.Size && !memcmp(item->Oid.Data,oid.Data,oid.Size))return xl6X509AttributeCopyV1(item);
    }
    return NULL;
}
void* xl6X509NameFindOidV1(void* raw,const xvalue* value)
{
    const XL6_X509NameV1* self=xl6X509NameCheckedV1(raw);xbytesview oid;
    return self && xl6X509OidInputV1(value,&oid)?find(self,oid):NULL;
}
void* xl6X509NameFindV1(void* raw,xstrview text)
{
    const XL6_X509NameV1* self=xl6X509NameCheckedV1(raw);if(!self)return NULL;
    xbuffer oid={0};void* result=xl6X509OidEncodeV1(text,&oid)?find(self,xrtBufferView(&oid)):NULL;
    xl6X509BufferFreeV1(&oid);
    if(xrtGetError()){xl6X509AttributeDestroyV1(result);return NULL;}
    return result;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509NameHandlePolicyV1Get,xl6X509NameDestroyV1)
