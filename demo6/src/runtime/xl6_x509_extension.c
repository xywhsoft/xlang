#include "xl6_x509_records_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509ExtensionDestroyV1(void* raw){xl6X509FreeV1(raw);}
XL6_X509ExtensionV1* xl6X509ExtensionCopyV1(const xx509ext* input)
{
    if(!input){xl6X509FailV1(XERR_STATE,"missing parsed X.509 extension");return NULL;}
    if(!xl6X509SpanV1(input->Raw))return NULL;
    xbytesview owned;XL6_X509ExtensionV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,input->Raw,NULL,&owned);
    if(!self)return NULL;
    self->value=*input;self->value.Raw=owned;
    bool ok=xl6X509RebaseV1(input->Raw,owned,input->Oid,&self->value.Oid) &&
        xl6X509RebaseV1(input->Raw,owned,input->Value,&self->value.Value);
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509ExtensionDestroyV1(self);return NULL;
}
xvalue* xl6X509ExtensionToDerV1(void* raw)
{const XL6_X509ExtensionV1* self=xl6X509ExtensionCheckedV1(raw);return self?xl6X509BytesV1(self->value.Raw):NULL;}
xvalue* xl6X509ExtensionOidContentV1(void* raw)
{const XL6_X509ExtensionV1* self=xl6X509ExtensionCheckedV1(raw);return self?xl6X509BytesV1(self->value.Oid):NULL;}
xstrview xl6X509ExtensionOidTextV1(void* raw)
{const XL6_X509ExtensionV1* self=xl6X509ExtensionCheckedV1(raw);return self?xl6X509OidTextV1(self->value.Oid):(xstrview){0};}
bool xl6X509ExtensionCriticalV1(void* raw)
{const XL6_X509ExtensionV1* self=xl6X509ExtensionCheckedV1(raw);return self && self->value.Critical;}
xvalue* xl6X509ExtensionValueV1(void* raw)
{const XL6_X509ExtensionV1* self=xl6X509ExtensionCheckedV1(raw);return self?xl6X509BytesV1(self->value.Value):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509ExtensionHandlePolicyV1Get,xl6X509ExtensionDestroyV1)
