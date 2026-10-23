#include "xl6_x509_profiles_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509KeyUsageDestroyV1(void* raw){xl6X509FreeV1(raw);}
int64_t xl6X509KeyUsageFlagsV1(void* raw)
{const XL6_X509KeyUsageV1* self=xl6X509ProfileCheckedV1(raw);return self?(int64_t)self->flags:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509KeyUsageHandlePolicyV1Get,xl6X509KeyUsageDestroyV1)
