#include "xl6_x509_path_internal.h"
#include "xl6_text_internal.h"
void* xl6X509PathOutcomePathV1(void* raw)
{
    const XL6_X509PathOutcomeV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing path outcome");return NULL;}
    return self->path?xl6X509PathRefV1(self->path):NULL;
}
void xl6X509PathOutcomeVerifyV1(void* raw){(void)xl6X509PathOutcomeRequireV1(raw);}
static const XL6_X509PathV1* checked(void* raw)
{if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing validated path");return raw;}
int64_t xl6X509PathCountV1(void* raw){const XL6_X509PathV1* p=checked(raw);return p?(int64_t)p->count:0;}
int64_t xl6X509PathTimeV1(void* raw){const XL6_X509PathV1* p=checked(raw);return p?p->policy.Time:0;}
int64_t xl6X509PathFlagsV1(void* raw){const XL6_X509PathV1* p=checked(raw);return p?p->policy.Flags:0;}
int64_t xl6X509PathUsageV1(void* raw){const XL6_X509PathV1* p=checked(raw);return p?p->policy.KeyUsage:0;}
xstrview xl6X509PathPurposeV1(void* raw)
{
    const XL6_X509PathV1* p=checked(raw);
    if(!p)return (xstrview){0};
    return p->policy.Purpose.Size?xl6X509OidTextV1(p->policy.Purpose):xl6TextOwn(xrtCalloc(1,1),0);
}

void* xl6X509PathCertificateV1(void* raw,int64_t index)
{
    const XL6_X509PathV1* self=raw;size_t selected;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing validated path");return NULL;}
    return xl6X509IndexV1(index,self->count,&selected)?
        xl6X509CertificateCreateSpanV1(self->certificates[selected].Raw):NULL;
}
void* xl6X509PathAnchorV1(void* raw)
{
    const XL6_X509PathV1* self=raw;
    if(!self){xl6X509FailV1(XERR_ARGUMENT,"missing validated path");return NULL;}
    XL6_X509CertificateV1 certificate={.view=self->anchorCertificate};
    return xl6X509AnchorCreateV1(&certificate);
}
