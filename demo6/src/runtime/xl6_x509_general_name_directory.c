#include "xl6_x509_profiles_internal.h"
void* xl6X509GeneralNameDirectoryV1(void* raw,int64_t limit)
{
    const XL6_X509GeneralNameV1* self=xl6X509ProfileCheckedV1(raw);
    if(!self || !xl6X509LimitV1(0,limit,"invalid directory Name attribute budget"))return NULL;
    if(self->value.Type!=X509_NAME_DIRECTORY) {
        xl6X509FailV1(XERR_STATE,"GeneralName is not a directory Name");return NULL;
    }
    return xl6X509NameCopyV1(self->value.Value,limit);
}
