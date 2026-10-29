#include "xl6_x509_crl_profiles_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509CrlEntriesDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509CrlEntriesCopyV1(xbytesview content,xx509crlversion version,int64_t limit)
{
    xx509crlversion checked;
    if(!xl6X509CrlVersionCheckV1(version,&checked) || !xl6X509LimitV1(0,limit,"invalid CRL entry budget"))return NULL;
    xx509crlcursor cursor={.Version=checked};xx509crlentry item;size_t count=0;
    if(!xl6X509OutcomeV1(xrtDerInit(&cursor.Items,content.Data,content.Size)))return NULL;
    for(;;) {
        xx509result status=xrtX509CrlEntryRead(&cursor,&item);if(status==X509_DONE)break;
        if(!xl6X509OutcomeV1(status==X509_VALUE))return NULL;
        if(count==SIZE_MAX){xl6X509FailV1(XERR_RANGE,"CRL entry count overflow");return NULL;}
        if(!xl6X509LimitV1(++count,limit,"CRL entry budget exceeded"))return NULL;
    }
    void* items=NULL;xbytesview owned;
    XL6_X509CrlEntriesV1* self=xl6X509RecordAllocV1(sizeof(*self),count,sizeof(xx509crlentry),_Alignof(xx509crlentry),content,&items,&owned);
    if(!self)return NULL;
    self->count=count;self->version=checked;self->content=owned;self->items=items;
    bool ok=xrtDerInit(&cursor.Items,owned.Data,owned.Size);cursor.Version=checked;
    for(size_t i=0;ok && i<count;++i)ok=xrtX509CrlEntryRead(&cursor,&self->items[i])==X509_VALUE;
    if(ok)ok=xrtX509CrlEntryRead(&cursor,&item)==X509_DONE;
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509CrlEntriesDestroyV1(self);return NULL;
}
void* xl6X509CrlEntriesFromDerV1(const xvalue* value,int64_t version,int64_t bytes,int64_t limit)
{
    xbytesview input,content;xx509crlversion checked;
    return xl6X509CrlVersionCheckV1(version,&checked) && xl6X509ProfileInputV1(value,bytes,&input) &&
        xl6X509SequenceContentV1(input,&content)?xl6X509CrlEntriesCopyV1(content,checked,limit):NULL;
}
xvalue* xl6X509CrlEntriesToDerV1(void* raw)
{const XL6_X509CrlEntriesV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509EncodeContentV1(XASN1_SEQUENCE,self->content):NULL;}
uint64_t xl6X509CrlEntriesCountV1(void* raw)
{const XL6_X509CrlEntriesV1* self=xl6X509ProfileCheckedV1(raw);return self?(uint64_t)self->count:0;}
int64_t xl6X509CrlEntriesVersionV1(void* raw)
{const XL6_X509CrlEntriesV1* self=xl6X509ProfileCheckedV1(raw);return self?(int64_t)self->version:0;}
void* xl6X509CrlEntriesAtV1(void* raw,int64_t index)
{
    const XL6_X509CrlEntriesV1* self=xl6X509ProfileCheckedV1(raw);size_t selected;
    return self && xl6X509IndexV1(index,self->count,&selected)?xl6X509CrlEntryCopyV1(self->items[selected].Raw,self->version):NULL;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509CrlEntriesHandlePolicyV1Get,xl6X509CrlEntriesDestroyV1)
