#include "xl6_x509_records_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509AttributeDestroyV1(void* raw){xl6X509FreeV1(raw);}
XL6_X509AttributeV1* xl6X509AttributeCopyV1(const xx509nameattr* input)
{
    if(!input){xl6X509FailV1(XERR_STATE,"missing parsed X.509 attribute");return NULL;}
    if(!xl6X509SpanV1(input->Raw))return NULL;
    xbytesview owned;XL6_X509AttributeV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,input->Raw,NULL,&owned);
    if(!self)return NULL;
    self->value=*input;self->value.Raw=owned;
    bool ok=xl6X509RebaseV1(input->Raw,owned,input->Oid,&self->value.Oid) &&
        xl6X509RebaseV1(input->Raw,owned,input->Value,&self->value.Value);
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509AttributeDestroyV1(self);return NULL;
}
xvalue* xl6X509AttributeToDerV1(void* raw)
{const XL6_X509AttributeV1* self=xl6X509AttributeCheckedV1(raw);return self?xl6X509BytesV1(self->value.Raw):NULL;}
xvalue* xl6X509AttributeOidContentV1(void* raw)
{const XL6_X509AttributeV1* self=xl6X509AttributeCheckedV1(raw);return self?xl6X509BytesV1(self->value.Oid):NULL;}
xstrview xl6X509AttributeOidTextV1(void* raw)
{const XL6_X509AttributeV1* self=xl6X509AttributeCheckedV1(raw);return self?xl6X509OidTextV1(self->value.Oid):(xstrview){0};}
uint64_t xl6X509AttributeRdnV1(void* raw)
{const XL6_X509AttributeV1* self=xl6X509AttributeCheckedV1(raw);return self?(uint64_t)self->value.Rdn:0;}
bool xl6X509AttributeValueSpanV1(void* raw,xbytesview* output)
{
    const XL6_X509AttributeV1* self=xl6X509AttributeCheckedV1(raw);if(!self)return false;
    xdercursor root,fields;xdervalue sequence,oid,value;
    bool ok=xrtDerInit(&root,self->value.Raw.Data,self->value.Raw.Size) && xrtDerRead(&root,&sequence)==XDER_VALUE &&
        xrtDerDone(&root) && xrtDerIs(&sequence,XASN1_UNIVERSAL,XASN1_SEQUENCE,true) && xrtDerEnter(&sequence,&fields) &&
        xrtDerRead(&fields,&oid)==XDER_VALUE && xrtDerRead(&fields,&value)==XDER_VALUE && xrtDerDone(&fields);
    if(!xl6X509OutcomeV1(ok))return false;
    *output=value.Raw;return true;
}
xvalue* xl6X509AttributeValueDerV1(void* raw)
{xbytesview value;return xl6X509AttributeValueSpanV1(raw,&value)?xl6X509BytesV1(value):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509AttributeHandlePolicyV1Get,xl6X509AttributeDestroyV1)
