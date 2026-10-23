#include "xl6_x509_profiles_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509AuthorityKeyIdentifierDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509AuthorityKeyIdentifierCopyV1(xbytesview raw)
{
    if(!xl6X509SpanV1(raw))return NULL;
    xbytesview owned;XL6_X509AuthorityKeyIdentifierV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,raw,NULL,&owned);
    if(!self)return NULL;
    self->raw=owned;
    if(xl6X509OutcomeV1(xrtX509AuthorityKeyIdParse(owned,&self->value)))return self;
    xl6X509AuthorityKeyIdentifierDestroyV1(self);return NULL;
}
void* xl6X509AuthorityKeyIdentifierFromDerV1(const xvalue* value,int64_t limit)
{xbytesview input;return xl6X509ProfileInputV1(value,limit,&input)?xl6X509AuthorityKeyIdentifierCopyV1(input):NULL;}
xvalue* xl6X509AuthorityKeyIdentifierToDerV1(void* raw)
{const XL6_X509AuthorityKeyIdentifierV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->raw):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509AuthorityKeyIdentifierHandlePolicyV1Get,xl6X509AuthorityKeyIdentifierDestroyV1)
bool xl6X509AuthorityKeyIdentifierHasKeyIdV1(void* raw)
{const XL6_X509AuthorityKeyIdentifierV1* self=xl6X509ProfileCheckedV1(raw);return self?self->value.HasKeyId:false;}
xvalue* xl6X509AuthorityKeyIdentifierKeyIdV1(void* raw)
{const XL6_X509AuthorityKeyIdentifierV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.KeyId):NULL;}
bool xl6X509AuthorityKeyIdentifierHasIssuerV1(void* raw)
{const XL6_X509AuthorityKeyIdentifierV1* self=xl6X509ProfileCheckedV1(raw);return self?self->value.HasIssuer:false;}
bool xl6X509AuthorityKeyIdentifierHasSerialV1(void* raw)
{const XL6_X509AuthorityKeyIdentifierV1* self=xl6X509ProfileCheckedV1(raw);return self?self->value.HasSerial:false;}
xvalue* xl6X509AuthorityKeyIdentifierSerialV1(void* raw)
{const XL6_X509AuthorityKeyIdentifierV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Serial):NULL;}
void* xl6X509AuthorityKeyIdentifierIssuerV1(void* raw,int64_t limit)
{
    const XL6_X509AuthorityKeyIdentifierV1* self=xl6X509ProfileCheckedV1(raw);
    if(!self || !xl6X509LimitV1(0,limit,"invalid issuer name budget"))return NULL;
    return self->value.HasIssuer?xl6X509GeneralNamesCopyV1(xl6X509CursorContentV1(self->value.Issuer.Items),limit):NULL;
}
