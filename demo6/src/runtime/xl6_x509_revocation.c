#include "xl6_x509_crl_policy_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509RevocationDestroyV1(void* raw)
{
    XL6_X509RevocationV1* self=raw;
    if(self && xrtAtomic64FetchSub(&self->references,1,XMEMORY_ACQ_REL)==1)xl6X509FreeV1(self);
}
void* xl6X509RevocationRefV1(void* raw)
{
    XL6_X509RevocationV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing revocation result");return NULL;}
    return xl6X509PolicyRetainV1(&self->references)?self:NULL;
}
void* xl6X509RevocationCopyV1(const xx509revocation* value,xx509crlversion version,bool observed)
{
    if(!value){xl6X509FailV1(XERR_ARGUMENT,"missing revocation output");return NULL;}
    if((value->CoveredReasons&~X509_CRL_REASON_FLAG_ALL) ||
        (observed && (value->State<X509_REVOCATION_GOOD || value->State>X509_REVOCATION_REMOVED)) ||
        (observed && !value->Entry.Raw.Size && (value->State!=X509_REVOCATION_GOOD ||
            value->Entry.Serial.Size || value->Entry.Extensions.Size || value->HasReason || value->HasInvalidityDate))) {
        xl6X509FailV1(XERR_STATE,"invalid native revocation result");return NULL;
    }
    xbytesview source=observed?value->Entry.Raw:(xbytesview){0},owned;
    XL6_X509RevocationV1* self=xl6X509RecordAllocV1(sizeof(*self),0,0,1,source,NULL,&owned);
    if(!self)return NULL;
    xrtAtomic64Init(&self->references,1);self->observed=observed;self->version=version;
    if(observed)self->value=*value;
    else self->value.CoveredReasons=value->CoveredReasons;
    if(observed && source.Size) {
        bool ok=xl6X509RebaseV1(source,owned,value->Entry.Raw,&self->value.Entry.Raw) &&
            xl6X509RebaseV1(source,owned,value->Entry.Serial,&self->value.Entry.Serial) &&
            xl6X509RebaseV1(source,owned,value->Entry.Extensions,&self->value.Entry.Extensions);
        if(!ok){xl6X509RevocationDestroyV1(self);return NULL;}
    }
    return self;
}
static const XL6_X509RevocationV1* checked(void* raw)
{
    if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing revocation result receiver");
    return raw;
}
int64_t xl6X509RevocationStatusV1(void* raw)
{
    const XL6_X509RevocationV1* self=checked(raw);
    if(!self || !self->observed)return XL6_X509_REVOCATION_UNKNOWN_V1;
    if(self->value.State==X509_REVOCATION_REVOKED)return XL6_X509_REVOCATION_REVOKED_V1;
    if(self->value.State==X509_REVOCATION_REMOVED)return XL6_X509_REVOCATION_REMOVED_V1;
    if(self->value.State!=X509_REVOCATION_GOOD) {
        xl6X509FailV1(XERR_STATE,"unknown native revocation state");return XL6_X509_REVOCATION_UNKNOWN_V1;
    }
    return self->value.CoveredReasons==X509_CRL_REASON_FLAG_ALL?XL6_X509_REVOCATION_GOOD_V1:XL6_X509_REVOCATION_UNKNOWN_V1;
}
uint64_t xl6X509RevocationCoverageV1(void* raw)
{const XL6_X509RevocationV1* self=checked(raw);return self?self->value.CoveredReasons:0;}
int64_t xl6X509RevocationReasonsV1(void* raw)
{return (int64_t)xl6X509RevocationCoverageV1(raw);}
bool xl6X509RevocationObservedV1(void* raw)
{const XL6_X509RevocationV1* self=checked(raw);return self && (self->observed || self->value.CoveredReasons!=0);}
void* xl6X509RevocationEntryV1(void* raw)
{
    const XL6_X509RevocationV1* self=checked(raw);
    return self && self->observed && self->value.Entry.Raw.Size?xl6X509CrlEntryCopyV1(self->value.Entry.Raw,self->version):NULL;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509RevocationHandlePolicyV1Get,xl6X509RevocationDestroyV1)
