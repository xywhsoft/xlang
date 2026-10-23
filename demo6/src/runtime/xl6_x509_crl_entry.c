#include "xl6_x509_crl_profiles_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509CrlEntryDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509CrlEntryCopyV1(xbytesview raw,xx509crlversion version)
{
    xx509crlversion checked;
    if(!xl6X509SpanV1(raw) || !xl6X509CrlVersionCheckV1(version,&checked))return NULL;
    xbytesview owned;XL6_X509CrlEntryV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,raw,NULL,&owned);
    if(!self)return NULL;
    self->version=checked;xx509crlcursor cursor={.Version=checked};
    bool ok=xrtDerInit(&cursor.Items,owned.Data,owned.Size) &&
        xrtX509CrlEntryRead(&cursor,&self->value)==X509_VALUE && xrtDerDone(&cursor.Items);
    if(xl6X509OutcomeV1(ok))return self;
    xl6X509CrlEntryDestroyV1(self);return NULL;
}
void* xl6X509CrlEntryFromDerV1(const xvalue* value,int64_t version,int64_t limit)
{
    xbytesview input;xx509crlversion checked;
    return xl6X509CrlVersionCheckV1(version,&checked) && xl6X509ProfileInputV1(value,limit,&input)?
        xl6X509CrlEntryCopyV1(input,checked):NULL;
}
xvalue* xl6X509CrlEntryToDerV1(void* raw)
{const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Raw):NULL;}
int64_t xl6X509CrlEntryVersionV1(void* raw)
{const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);return self?(int64_t)self->version:0;}
xvalue* xl6X509CrlEntrySerialV1(void* raw)
{const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Serial):NULL;}
int64_t xl6X509CrlEntryRevokedAtV1(void* raw)
{const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);return self?self->value.RevokedAt:0;}
xvalue* xl6X509CrlEntryExtensionsDerV1(void* raw)
{const XL6_X509CrlEntryV1* self=xl6X509ProfileCheckedV1(raw);return self?xl6X509BytesV1(self->value.Extensions):NULL;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509CrlEntryHandlePolicyV1Get,xl6X509CrlEntryDestroyV1)
