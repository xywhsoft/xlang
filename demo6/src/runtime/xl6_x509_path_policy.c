#include "xl6_x509_path_internal.h"
#include "xl6_text_internal.h"
#include "src/api/xl_foreign_error.h"
void xl6X509PathPolicyDestroyV1(void* raw){xl6X509FreeV1(raw);}
void* xl6X509PathPolicyCreateV1(int64_t at,int64_t flags,int64_t usage,xstrview purpose,int64_t maxCertificates,int64_t maxBytes,int64_t maxSearchSteps)
{
    if(xrtGetError())return NULL;
    if(flags<0 || (uint64_t)flags>7 || usage<0 || (uint64_t)usage>511 || maxCertificates<0 || maxBytes<0 || maxSearchSteps<0) {
        xl6X509FailV1(XERR_ARGUMENT,"invalid explicit path policy or budget");return NULL;
    }
    if((uint64_t)maxSearchSteps>SIZE_MAX){xl6X509FailV1(XERR_RANGE,"path search budget exceeds native size range");return NULL;}
    if((purpose.Size && !purpose.Data) || purpose.Size>INT64_MAX) {
        xl6X509FailV1(XERR_ARGUMENT,"invalid purpose OID text");return NULL;
    }
    if(!xl6X509LimitV1(purpose.Size,maxBytes,"purpose text exceeds policy budget"))return NULL;
    xbuffer encoded={0};bool ok=!purpose.Size || xl6X509OidEncodeV1(purpose,&encoded);
    xx509pathconfig config={.Time=at,.Flags=(uint32)flags,.KeyUsage=(uint16)usage,.Purpose={encoded.Data,encoded.Size}};
    if(ok)ok=xl6X509PathPolicyValidV1(&config) && xl6X509LimitV1(encoded.Size,maxBytes,"purpose OID exceeds policy budget");
    if(ok && encoded.Size>SIZE_MAX-sizeof(XL6_X509PathPolicyV1))ok=xl6X509FailV1(XERR_RANGE,"path policy size overflow");
    XL6_X509PathPolicyV1* self=ok?xrtCalloc(1,sizeof(*self)+encoded.Size):NULL;
    if(self) {
        self->value=config;self->maxCertificates=maxCertificates;self->maxBytes=maxBytes;self->maxSearchSteps=maxSearchSteps;
        if(encoded.Size){memcpy(self->data,encoded.Data,encoded.Size);self->value.Purpose=(xbytesview){self->data,encoded.Size};}
    }
    xl6X509BufferFreeV1(&encoded);return self;
}
static const XL6_X509PathPolicyV1* checked(void* raw)
{if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing path policy");return raw;}
int64_t xl6X509PathPolicyTimeV1(void* raw){const XL6_X509PathPolicyV1* p=checked(raw);return p?p->value.Time:0;}
int64_t xl6X509PathPolicyFlagsV1(void* raw){const XL6_X509PathPolicyV1* p=checked(raw);return p?p->value.Flags:0;}
int64_t xl6X509PathPolicyUsageV1(void* raw){const XL6_X509PathPolicyV1* p=checked(raw);return p?p->value.KeyUsage:0;}
int64_t xl6X509PathPolicyMaxCertificatesV1(void* raw){const XL6_X509PathPolicyV1* p=checked(raw);return p?p->maxCertificates:0;}
int64_t xl6X509PathPolicyMaxBytesV1(void* raw){const XL6_X509PathPolicyV1* p=checked(raw);return p?p->maxBytes:0;}
int64_t xl6X509PathPolicyMaxSearchStepsV1(void* raw){const XL6_X509PathPolicyV1* p=checked(raw);return p?p->maxSearchSteps:0;}
xstrview xl6X509PathPolicyPurposeV1(void* raw)
{
    const XL6_X509PathPolicyV1* p=checked(raw);
    if(!p)return (xstrview){0};
    return p->value.Purpose.Size?xl6X509OidTextV1(p->value.Purpose):xl6TextOwn(xrtCalloc(1,1),0);
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509PathPolicyHandlePolicyV1Get,xl6X509PathPolicyDestroyV1)
