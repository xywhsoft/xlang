#include "xl6_x509_crl_profiles_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509IssuingDistributionPointDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509IssuingDistributionPointCopyV1(xbytesview raw)
{
    if(!xl6X509SpanV1(raw))return NULL;
    xbytesview owned;
    XL6_X509IssuingDistributionPointV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,raw,NULL,&owned);
    if(!self)return NULL;
    self->raw=owned;
    if(xl6X509OutcomeV1(xrtX509IssuingPointParse(owned,&self->value)))return self;
    xl6X509IssuingDistributionPointDestroyV1(self);return NULL;
}
void* xl6X509IssuingDistributionPointFromDerV1(const xvalue* value,int64_t limit)
{xbytesview input;return xl6X509ProfileInputV1(value,limit,&input)?xl6X509IssuingDistributionPointCopyV1(input):NULL;}
xvalue* xl6X509IssuingDistributionPointToDerV1(void* raw)
{const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->raw):NULL;}
bool xl6X509IssuingDistributionPointHasNameV1(void* raw)
{const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasDistributionPoint;}
int64_t xl6X509IssuingDistributionPointNameKindV1(void* raw)
{
    const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return 0;
    if(!self->value.HasDistributionPoint){xl6X509FailV1(XERR_STATE,"IssuingDistributionPoint has no name");return 0;}
    return (int64_t)self->value.DistributionPoint.Type;
}
xvalue* xl6X509IssuingDistributionPointNameDerV1(void* raw)
{const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.DistributionPoint.Raw):NULL;}
bool xl6X509IssuingDistributionPointOnlyUsersV1(void* raw)
{const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.OnlyUserCertificates;}
bool xl6X509IssuingDistributionPointOnlyCasV1(void* raw)
{const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.OnlyCaCertificates;}
bool xl6X509IssuingDistributionPointHasReasonsV1(void* raw)
{const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasReasons;}
int64_t xl6X509IssuingDistributionPointReasonsV1(void* raw)
{const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self?(int64_t)self->value.Reasons:0;}
bool xl6X509IssuingDistributionPointIndirectV1(void* raw)
{const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.Indirect;}
bool xl6X509IssuingDistributionPointOnlyAttributesV1(void* raw)
{const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.OnlyAttributeCertificates;}
xvalue* xl6X509IssuingDistributionPointRelativeNameV1(void* raw)
{
    const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return NULL;
    if(!self->value.HasDistributionPoint || self->value.DistributionPoint.Type!=X509_DISTRIBUTION_RELATIVE_NAME) {
        xl6X509FailV1(XERR_STATE,"IssuingDistributionPoint does not have a relative RDN");return NULL;
    }
    return xl6X509EncodeContentV1(XASN1_SET,self->value.DistributionPoint.Value);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509IssuingDistributionPointHandlePolicyV1Get,xl6X509IssuingDistributionPointDestroyV1)
