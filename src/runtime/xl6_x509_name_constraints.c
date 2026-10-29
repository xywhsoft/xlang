#include "xl6_x509_profiles_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509NameConstraintsDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509NameConstraintsCopyV1(xbytesview raw)
{
    if(!xl6X509SpanV1(raw))return NULL;
    xbytesview owned;XL6_X509NameConstraintsV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,raw,NULL,&owned);
    if(!self)return NULL;
    self->raw=owned;
    if(xl6X509OutcomeV1(xrtX509NameConstraintsParse(owned,&self->value)))return self;
    xl6X509NameConstraintsDestroyV1(self);return NULL;
}
void* xl6X509NameConstraintsFromDerV1(const xvalue* value,int64_t limit)
{xbytesview input;return xl6X509ProfileInputV1(value,limit,&input)?xl6X509NameConstraintsCopyV1(input):NULL;}
xvalue* xl6X509NameConstraintsToDerV1(void* raw)
{const XL6_X509NameConstraintsV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->raw):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509NameConstraintsHandlePolicyV1Get,xl6X509NameConstraintsDestroyV1)
bool xl6X509NameConstraintsHasPermittedV1(void* raw)
{const XL6_X509NameConstraintsV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasPermitted;}
void* xl6X509NameConstraintsPermittedV1(void* raw,int64_t limit)
{
    const XL6_X509NameConstraintsV1* self=xl6X509ProfileCheckedV1(raw);
    if(!self || !xl6X509LimitV1(0,limit,"invalid name constraint item budget"))return NULL;
    return self->value.HasPermitted?xl6X509GeneralSubtreesCopyV1(xl6X509CursorContentV1(self->value.Permitted.Items),limit):NULL;
}
bool xl6X509NameConstraintsHasExcludedV1(void* raw)
{const XL6_X509NameConstraintsV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasExcluded;}
void* xl6X509NameConstraintsExcludedV1(void* raw,int64_t limit)
{
    const XL6_X509NameConstraintsV1* self=xl6X509ProfileCheckedV1(raw);
    if(!self || !xl6X509LimitV1(0,limit,"invalid name constraint item budget"))return NULL;
    return self->value.HasExcluded?xl6X509GeneralSubtreesCopyV1(xl6X509CursorContentV1(self->value.Excluded.Items),limit):NULL;
}
