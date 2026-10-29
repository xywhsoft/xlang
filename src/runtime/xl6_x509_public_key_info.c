#include "xl6_x509_records_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509PublicKeyInfoDestroyV1(void* raw){xl6X509FreeV1(raw);}
XL6_X509PublicKeyInfoV1* xl6X509PublicKeyInfoCopyV1(xbytesview input,const xx509pubkey* key)
{
    if(!key){xl6X509FailV1(XERR_STATE,"missing parsed X.509 public key");return NULL;}
    if(!xl6X509SpanV1(input))return NULL;
    xbytesview owned;XL6_X509PublicKeyInfoV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,input,NULL,&owned);
    if(!self)return NULL;
    self->value=*key;self->raw=owned;
    bool ok=xl6X509RebaseV1(input,owned,key->Algorithm.Raw,&self->value.Algorithm.Raw) &&
        xl6X509RebaseV1(input,owned,key->Algorithm.Oid,&self->value.Algorithm.Oid) &&
        xl6X509RebaseV1(input,owned,key->Algorithm.Parameters,&self->value.Algorithm.Parameters) &&
        xl6X509RebaseV1(input,owned,key->Key,&self->value.Key) &&
        xl6X509RebaseV1(input,owned,key->Modulus,&self->value.Modulus) &&
        xl6X509RebaseV1(input,owned,key->Exponent,&self->value.Exponent);
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509PublicKeyInfoDestroyV1(self);return NULL;
}
xvalue* xl6X509PublicKeyInfoSpkiV1(void* raw)
{const XL6_X509PublicKeyInfoV1* self=xl6X509PublicKeyInfoCheckedV1(raw);return self?xl6X509BytesV1(self->raw):NULL;}
int64_t xl6X509PublicKeyInfoKindV1(void* raw)
{const XL6_X509PublicKeyInfoV1* self=xl6X509PublicKeyInfoCheckedV1(raw);return self?(int64_t)self->value.Type:0;}
int64_t xl6X509PublicKeyInfoCurveV1(void* raw)
{const XL6_X509PublicKeyInfoV1* self=xl6X509PublicKeyInfoCheckedV1(raw);return self?(int64_t)self->value.Curve:0;}
void* xl6X509PublicKeyInfoAlgorithmV1(void* raw)
{const XL6_X509PublicKeyInfoV1* self=xl6X509PublicKeyInfoCheckedV1(raw);return self?xl6X509AlgorithmCopyV1(&self->value.Algorithm):NULL;}
xvalue* xl6X509PublicKeyInfoKeyBytesV1(void* raw)
{const XL6_X509PublicKeyInfoV1* self=xl6X509PublicKeyInfoCheckedV1(raw);return self?xl6X509BytesV1(self->value.Key):NULL;}
xvalue* xl6X509PublicKeyInfoModulusV1(void* raw)
{const XL6_X509PublicKeyInfoV1* self=xl6X509PublicKeyInfoCheckedV1(raw);return self?xl6X509BytesV1(self->value.Modulus):NULL;}
xvalue* xl6X509PublicKeyInfoExponentV1(void* raw)
{const XL6_X509PublicKeyInfoV1* self=xl6X509PublicKeyInfoCheckedV1(raw);return self?xl6X509BytesV1(self->value.Exponent):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509PublicKeyInfoHandlePolicyV1Get,xl6X509PublicKeyInfoDestroyV1)
