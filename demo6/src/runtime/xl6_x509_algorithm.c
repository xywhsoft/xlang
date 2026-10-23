#include "xl6_x509_records_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509AlgorithmDestroyV1(void* raw){xl6X509FreeV1(raw);}
XL6_X509AlgorithmV1* xl6X509AlgorithmCopyV1(const xx509algorithm* input)
{
    if(!input){xl6X509FailV1(XERR_STATE,"missing parsed X.509 algorithm");return NULL;}
    if(!xl6X509SpanV1(input->Raw))return NULL;
    xbytesview owned;XL6_X509AlgorithmV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,input->Raw,NULL,&owned);
    if(!self)return NULL;
    self->value=*input;self->value.Raw=owned;
    bool ok=xl6X509RebaseV1(input->Raw,owned,input->Oid,&self->value.Oid) &&
        xl6X509RebaseV1(input->Raw,owned,input->Parameters,&self->value.Parameters);
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509AlgorithmDestroyV1(self);return NULL;
}
void* xl6X509AlgorithmFromDerV1(const xvalue* value,int64_t limit)
{
    xbytesview input;xx509algorithm algorithm;
    return xl6X509InputV1(value,&input) && xl6X509LimitV1(input.Size,limit,"AlgorithmIdentifier DER exceeds input budget") &&
        xl6X509OutcomeV1(xrtX509AlgorithmParse(input,&algorithm))?xl6X509AlgorithmCopyV1(&algorithm):NULL;
}
xvalue* xl6X509AlgorithmToDerV1(void* raw)
{const XL6_X509AlgorithmV1* self=xl6X509AlgorithmCheckedV1(raw);return self?xl6X509BytesV1(self->value.Raw):NULL;}
xvalue* xl6X509AlgorithmOidContentV1(void* raw)
{const XL6_X509AlgorithmV1* self=xl6X509AlgorithmCheckedV1(raw);return self?xl6X509BytesV1(self->value.Oid):NULL;}
xstrview xl6X509AlgorithmOidTextV1(void* raw)
{const XL6_X509AlgorithmV1* self=xl6X509AlgorithmCheckedV1(raw);return self?xl6X509OidTextV1(self->value.Oid):(xstrview){0};}
bool xl6X509AlgorithmHasParametersV1(void* raw)
{const XL6_X509AlgorithmV1* self=xl6X509AlgorithmCheckedV1(raw);return self && self->value.HasParameters;}
xvalue* xl6X509AlgorithmParametersV1(void* raw)
{const XL6_X509AlgorithmV1* self=xl6X509AlgorithmCheckedV1(raw);return self?xl6X509BytesV1(self->value.Parameters):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509AlgorithmHandlePolicyV1Get,xl6X509AlgorithmDestroyV1)
