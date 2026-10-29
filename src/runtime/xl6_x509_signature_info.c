#include "xl6_x509_records_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509SignatureInfoDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509AlgorithmSignatureInfoV1(void* raw)
{
    const XL6_X509AlgorithmV1* self=xl6X509AlgorithmCheckedV1(raw);if(!self)return NULL;
    xx509signature signature;xx509result status=xrtX509SignatureParse(&self->value,&signature);
    if(status==X509_DONE)return NULL;
    if(!xl6X509OutcomeV1(status==X509_VALUE))return NULL;
    XL6_X509SignatureInfoV1* result=xrtMalloc(sizeof(*result));if(!result)return NULL;
    result->value=signature;return result;
}
int64_t xl6X509SignatureInfoKindV1(void* raw)
{const XL6_X509SignatureInfoV1* self=xl6X509SignatureInfoCheckedV1(raw);return self?(int64_t)self->value.Type:0;}
int64_t xl6X509SignatureInfoHashV1(void* raw)
{const XL6_X509SignatureInfoV1* self=xl6X509SignatureInfoCheckedV1(raw);return self?(int64_t)self->value.Hash:0;}
int64_t xl6X509SignatureInfoMaskHashV1(void* raw)
{const XL6_X509SignatureInfoV1* self=xl6X509SignatureInfoCheckedV1(raw);return self?(int64_t)self->value.MaskHash:0;}
uint64_t xl6X509SignatureInfoSaltBytesV1(void* raw)
{const XL6_X509SignatureInfoV1* self=xl6X509SignatureInfoCheckedV1(raw);return self?(uint64_t)self->value.SaltSize:0;}
uint64_t xl6X509SignatureInfoTrailerV1(void* raw)
{const XL6_X509SignatureInfoV1* self=xl6X509SignatureInfoCheckedV1(raw);return self?(uint64_t)self->value.Trailer:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509SignatureInfoHandlePolicyV1Get,xl6X509SignatureInfoDestroyV1)
