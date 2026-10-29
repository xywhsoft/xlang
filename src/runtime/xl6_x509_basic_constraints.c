#include "xl6_x509_profiles_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509BasicConstraintsDestroyV1(void* raw){xl6X509FreeV1(raw);}
bool xl6X509BasicConstraintsCaV1(void* raw)
{const XL6_X509BasicConstraintsV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.CA;}
bool xl6X509BasicConstraintsHasPathLimitV1(void* raw)
{const XL6_X509BasicConstraintsV1* self=xl6X509ProfileCheckedV1(raw);return self && self->value.HasPathLimit;}
uint64_t xl6X509BasicConstraintsPathLimitV1(void* raw)
{
    const XL6_X509BasicConstraintsV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return 0;
    if(!self->value.HasPathLimit){xl6X509FailV1(XERR_STATE,"BasicConstraints has no path limit");return 0;}
    return (uint64_t)self->value.PathLimit;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509BasicConstraintsHandlePolicyV1Get,xl6X509BasicConstraintsDestroyV1)
