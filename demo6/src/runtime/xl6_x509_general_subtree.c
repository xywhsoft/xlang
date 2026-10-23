#include "xl6_x509_profiles_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509GeneralSubtreeDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509GeneralSubtreeCopyV1(xbytesview raw)
{
    if(!xl6X509SpanV1(raw))return NULL;
    xbytesview owned;XL6_X509GeneralSubtreeV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,raw,NULL,&owned);
    if(!self)return NULL;
    self->value.Raw=owned;xx509subtreecursor cursor;
    if(xl6X509OutcomeV1((xrtDerInit(&cursor.Items,owned.Data,owned.Size) && xrtX509SubtreeRead(&cursor,&self->value)==X509_VALUE && xrtDerDone(&cursor.Items))))return self;
    xl6X509GeneralSubtreeDestroyV1(self);return NULL;
}
void* xl6X509GeneralSubtreeFromDerV1(const xvalue* value,int64_t limit)
{xbytesview input;return xl6X509ProfileInputV1(value,limit,&input)?xl6X509GeneralSubtreeCopyV1(input):NULL;}
xvalue* xl6X509GeneralSubtreeToDerV1(void* raw)
{const XL6_X509GeneralSubtreeV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Raw):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509GeneralSubtreeHandlePolicyV1Get,xl6X509GeneralSubtreeDestroyV1)
void* xl6X509GeneralSubtreeBaseV1(void* raw)
{const XL6_X509GeneralSubtreeV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509GeneralNameCopyV1(&self->value.Base,true):NULL;}
bool xl6X509GeneralSubtreeHasMinimumV1(void* raw)
{const XL6_X509GeneralSubtreeV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasMinimum;}
xvalue* xl6X509GeneralSubtreeMinimumV1(void* raw)
{const XL6_X509GeneralSubtreeV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Minimum):NULL;}
bool xl6X509GeneralSubtreeHasMaximumV1(void* raw)
{const XL6_X509GeneralSubtreeV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasMaximum;}
xvalue* xl6X509GeneralSubtreeMaximumV1(void* raw)
{const XL6_X509GeneralSubtreeV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Maximum):NULL;}
