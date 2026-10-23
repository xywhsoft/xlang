#include "xl6_x509_crl_profiles_internal.h"
void* xl6X509CrlEntryExtensionsV1(void* raw,int64_t limit)
{const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509ExtensionsCopyV1(self->value.Extensions,limit):NULL;}
