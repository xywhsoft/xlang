#include "xl6_x509_crl_policy_internal.h"

static const XL6_X509ValidatedCrlV1* checked(void* raw)
{if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing validated CRL receiver");return raw;}
void* xl6X509ValidatedCrlCrlV1(void* raw)
{const XL6_X509ValidatedCrlV1* self=checked(raw);return self?xl6X509CrlCreateSpanV1(self->crl.Raw):NULL;}
void* xl6X509ValidatedCrlIssuerV1(void* raw)
{const XL6_X509ValidatedCrlV1* self=checked(raw);return self?xl6X509CertificateCreateSpanV1(self->issuer.Raw):NULL;}
int64_t xl6X509ValidatedCrlTimeV1(void* raw)
{const XL6_X509ValidatedCrlV1* self=checked(raw);return self?self->time:0;}
uint64_t xl6X509ValidatedCrlSkewV1(void* raw)
{const XL6_X509ValidatedCrlV1* self=checked(raw);return self?self->skew:0;}
int64_t xl6X509ValidatedCrlFlagsV1(void* raw)
{const XL6_X509ValidatedCrlV1* self=checked(raw);return self?self->flags:0;}
bool xl6X509ValidatedCrlDeltaV1(void* raw)
{const XL6_X509ValidatedCrlV1* self=checked(raw);return self && self->value.Delta;}
static const XL6_X509CrlSetV1* set(void* raw)
{if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing CRL set receiver");return raw;}
void* xl6X509CrlSetBaseV1(void* raw)
{const XL6_X509CrlSetV1* self=set(raw);return self?xl6X509ValidatedCrlRefV1(self->base):NULL;}
void* xl6X509CrlSetDeltaV1(void* raw)
{const XL6_X509CrlSetV1* self=set(raw);return self?xl6X509ValidatedCrlRefV1(self->delta):NULL;}
