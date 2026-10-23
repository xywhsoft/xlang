#include "xl6_x509_profiles_internal.h"
bool xl6X509GeneralNameWithinV1(void* raw,void* baseRaw)
{
    const XL6_X509GeneralNameV1* self=xl6X509ProfileCheckedV1(raw);
    const XL6_X509GeneralNameV1* base=xl6X509ProfileCheckedV1(baseRaw);
    if(!self || !base)return false;
    if(self->constraintBase || !base->constraintBase)
        return xl6X509FailV1(XERR_STATE,"isWithin requires an actual name and a constraint base");
    xx509result result=xrtX509GeneralNameWithin(&self->value,&base->value);
    if(result==X509_DONE)return false;
    return xl6X509OutcomeV1(result==X509_VALUE);
}
