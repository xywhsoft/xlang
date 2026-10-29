#include "xl6_x509_profiles_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509GeneralSubtreesDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509GeneralSubtreesCopyV1(xbytesview content,int64_t limit)
{
    if(!xl6X509LimitV1(0,limit,"invalid X.509 item budget"))return NULL;
    xx509subtreecursor cursor;xx509subtree item;size_t count=0;
    if(!xl6X509OutcomeV1(xrtDerInit(&cursor.Items,content.Data,content.Size)))return NULL;
    for(;;) {
        xx509result status=xrtX509SubtreeRead(&cursor,&item);
        if(status==X509_DONE)break;
        if(!xl6X509OutcomeV1(status==X509_VALUE))return NULL;
        if(count==SIZE_MAX){xl6X509FailV1(XERR_RANGE,"X.509 item count overflow");return NULL;}
        if(!xl6X509LimitV1(++count,limit,"X.509 item budget exceeded"))return NULL;
    }
    if(!count){xl6X509FailV1(XERR_PROTOCOL,"X.509 sequence must not be empty");return NULL;}
    void* items=NULL;xbytesview owned;
    XL6_X509GeneralSubtreesV1* self=xl6X509RecordAllocV1(sizeof(*self),count,sizeof(xx509subtree),_Alignof(xx509subtree),content,&items,&owned);
    if(!self)return NULL;
    self->count=count;self->content=owned;self->items=items;
    bool ok=xrtDerInit(&cursor.Items,owned.Data,owned.Size);
    for(size_t i=0;ok && i<count;++i)ok=xrtX509SubtreeRead(&cursor,&self->items[i])==X509_VALUE;
    if(ok)ok=xrtX509SubtreeRead(&cursor,&item)==X509_DONE;
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509GeneralSubtreesDestroyV1(self);return NULL;
}
void* xl6X509GeneralSubtreesFromDerV1(const xvalue* value,int64_t bytes,int64_t limit)
{
    xbytesview input;xx509subtreecursor cursor;xbytesview content;
    if(!xl6X509ProfileInputV1(value,bytes,&input) || !xl6X509SequenceContentV1(input,&content) || !xl6X509OutcomeV1(xrtDerInit(&cursor.Items,content.Data,content.Size)))return NULL;
    return xl6X509GeneralSubtreesCopyV1(xl6X509CursorContentV1(cursor.Items),limit);
}
xvalue* xl6X509GeneralSubtreesToDerV1(void* raw)
{const XL6_X509GeneralSubtreesV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509EncodeContentV1(XASN1_SEQUENCE,self->content):NULL;}
uint64_t xl6X509GeneralSubtreesCountV1(void* raw)
{const XL6_X509GeneralSubtreesV1* self=xl6X509ProfileCheckedV1(raw);return self?(uint64_t)self->count:0;}
void* xl6X509GeneralSubtreesAtV1(void* raw,int64_t index)
{
    const XL6_X509GeneralSubtreesV1* self=xl6X509ProfileCheckedV1(raw);size_t selected;
    return self && xl6X509IndexV1(index,self->count,&selected)?xl6X509GeneralSubtreeCopyV1(self->items[selected].Raw):NULL;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509GeneralSubtreesHandlePolicyV1Get,xl6X509GeneralSubtreesDestroyV1)
