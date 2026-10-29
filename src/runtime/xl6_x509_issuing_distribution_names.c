#include "xl6_x509_crl_profiles_internal.h"
void* xl6X509IssuingDistributionPointFullNamesV1(void* raw,int64_t limit)
{
    const XL6_X509IssuingDistributionPointV1* self=xl6X509ProfileCheckedV1(raw);
    if(!self || !xl6X509LimitV1(0,limit,"invalid issuing distribution name budget"))return NULL;
    return self->value.HasDistributionPoint && self->value.DistributionPoint.Type==X509_DISTRIBUTION_FULL_NAME?
        xl6X509GeneralNamesCopyV1(xl6X509CursorContentV1(self->value.DistributionPoint.FullNames.Items),limit):NULL;
}
