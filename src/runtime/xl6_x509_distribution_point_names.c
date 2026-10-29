#include "xl6_x509_profiles_internal.h"
void* xl6X509DistributionPointFullNamesV1(void* raw,int64_t limit)
{
    const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);
    if(!self || !xl6X509LimitV1(0,limit,"invalid distribution name budget"))return NULL;
    return self->value.HasName && self->value.Name.Type==X509_DISTRIBUTION_FULL_NAME?
        xl6X509GeneralNamesCopyV1(xl6X509CursorContentV1(self->value.Name.FullNames.Items),limit):NULL;
}
void* xl6X509DistributionPointIssuerV1(void* raw,int64_t limit)
{
    const XL6_X509DistributionPointV1* self=xl6X509ProfileCheckedV1(raw);
    if(!self || !xl6X509LimitV1(0,limit,"invalid distribution issuer budget"))return NULL;
    return self->value.HasIssuer?xl6X509GeneralNamesCopyV1(xl6X509CursorContentV1(self->value.Issuer.Items),limit):NULL;
}
