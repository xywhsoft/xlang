#include "xl6_x509_profiles_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509GeneralNameDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509GeneralNameCopyV1(const xx509genname* input,bool base)
{
    if(!input){xl6X509FailV1(XERR_STATE,"missing parsed GeneralName");return NULL;}
    if(!xl6X509SpanV1(input->Raw))return NULL;
    xbytesview owned;XL6_X509GeneralNameV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,input->Raw,NULL,&owned);
    if(!self)return NULL;
    self->value=*input;self->value.Raw=owned;self->constraintBase=base;
    if(xl6X509RebaseV1(input->Raw,owned,input->Value,&self->value.Value))return self;
    xl6X509GeneralNameDestroyV1(self);return NULL;
}
xvalue* xl6X509GeneralNameToDerV1(void* raw)
{const XL6_X509GeneralNameV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Raw):NULL;}
xvalue* xl6X509GeneralNameValueV1(void* raw)
{const XL6_X509GeneralNameV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Value):NULL;}
int64_t xl6X509GeneralNameKindV1(void* raw)
{const XL6_X509GeneralNameV1* self=xl6X509ProfileCheckedV1(raw);return self?(int64_t)self->value.Type:0;}
bool xl6X509GeneralNameConstraintBaseV1(void* raw)
{const XL6_X509GeneralNameV1* self=xl6X509ProfileCheckedV1(raw);return self && self->constraintBase;}
xstrview xl6X509GeneralNameTextV1(void* raw)
{
    const XL6_X509GeneralNameV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return (xstrview){0};
    if(self->value.Type!=X509_NAME_EMAIL && self->value.Type!=X509_NAME_DNS && self->value.Type!=X509_NAME_URI) {
        xl6X509FailV1(XERR_STATE,"GeneralName is not IA5 email, DNS or URI text");return (xstrview){0};
    }
    return xl6X509ProfileTextV1(self->value.Value);
}
xstrview xl6X509GeneralNameOidV1(void* raw)
{
    const XL6_X509GeneralNameV1* self=xl6X509ProfileCheckedV1(raw);if(!self)return (xstrview){0};
    if(self->value.Type!=X509_NAME_REGISTERED_ID) {
        xl6X509FailV1(XERR_STATE,"GeneralName is not a registered OID");return (xstrview){0};
    }
    return xl6X509OidTextV1(self->value.Value);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509GeneralNameHandlePolicyV1Get,xl6X509GeneralNameDestroyV1)
