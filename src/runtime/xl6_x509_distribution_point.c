#include "xl6_x509_profiles_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509DistributionPointDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509DistributionPointCopyV1(xbytesview raw)
{
    if(!xl6X509SpanV1(raw))return NULL;
    xbytesview owned;XL6_X509DistributionPointV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,raw,NULL,&owned);
    if(!self)return NULL;
    self->value.Raw=owned;
    if(xl6X509OutcomeV1(xrtX509DistributionPointParse(owned,&self->value)))return self;
    xl6X509DistributionPointDestroyV1(self);return NULL;
}
void* xl6X509DistributionPointFromDerV1(const xvalue* value,int64_t limit)
{xbytesview input;return xl6X509ProfileInputV1(value,limit,&input)?xl6X509DistributionPointCopyV1(input):NULL;}
xvalue* xl6X509DistributionPointToDerV1(void* raw)
{const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Raw):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509DistributionPointHandlePolicyV1Get,xl6X509DistributionPointDestroyV1)
bool xl6X509DistributionPointHasNameV1(void* raw)
{const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasName;}
bool xl6X509DistributionPointHasReasonsV1(void* raw)
{const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasReasons;}
bool xl6X509DistributionPointHasIssuerV1(void* raw)
{const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasIssuer;}
int64_t xl6X509DistributionPointNameKindV1(void* raw)
{
    const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return 0;
    if(!self->value.HasName){xl6X509FailV1(XERR_STATE,"DistributionPoint has no name");return 0;}
    return (int64_t)self->value.Name.Type;
}
xvalue* xl6X509DistributionPointNameDerV1(void* raw)
{const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Name.Raw):NULL;}
int64_t xl6X509DistributionPointReasonsV1(void* raw)
{const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self?(int64_t)self->value.Reasons:0;}
xvalue* xl6X509DistributionPointRelativeNameV1(void* raw)
{
    const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return NULL;
    if(!self->value.HasName || self->value.Name.Type!=X509_DISTRIBUTION_RELATIVE_NAME) {
        xl6X509FailV1(XERR_STATE,"DistributionPoint does not have a relative RDN");return NULL;
    }
    return xl6X509EncodeContentV1(XASN1_SET,self->value.Name.Value);
}
