#include "xl6_x509_records_internal.h"

static const xx509crl* view(void* raw)
{
    if(!raw){xl6X509FailV1(XERR_ARGUMENT,"missing Crl receiver");return NULL;}
    return &((XL6_X509CrlV1*)raw)->view;
}
void* xl6X509CrlExtensionListV1(void* raw,int64_t limit)
{const xx509crl* self=view(raw);return self?xl6X509ExtensionsCopyV1(self->Extensions,limit):NULL;}
