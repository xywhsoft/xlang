#include "xl6_x509_path_internal.h"
#include "src/api/xl_foreign_error.h"

void xl6X509PathOutcomeDestroyV1(void* raw)
{
    XL6_X509PathOutcomeV1* self=raw;
    if(!self)return;
    xerror* prior=xrtTakeError();
    xrtErrorFree(self->diagnostic);xl6X509PathDestroyV1(self->path);xl6X509FreeV1(self);
    if(prior){xrtClearError();xrtSetErrorTake(prior);}
}
static const XL6_X509PathOutcomeV1* checked(void* raw)
{if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing path outcome");return raw;}
int64_t xl6X509PathOutcomeStatusV1(void* raw)
{const XL6_X509PathOutcomeV1* self=checked(raw);return self?self->status:0;}
int64_t xl6X509PathOutcomeCodeV1(void* raw)
{const XL6_X509PathOutcomeV1* self=checked(raw);return self?self->code:0;}
int64_t xl6X509PathOutcomeDepthV1(void* raw)
{const XL6_X509PathOutcomeV1* self=checked(raw);return self?self->depth:-1;}
bool xl6X509PathOutcomeRequireV1(void* raw)
{
    const XL6_X509PathOutcomeV1* self=checked(raw);
    if(!self || xrtGetError())return false;
    if(self->status==XL6_X509_PATH_VALID_V1)return true;
    if(!self->diagnostic)return xl6X509FailV1(XERR_STATE,"negative path outcome has no diagnostic");
    xrtSetErrorTake(xrtErrorRef(self->diagnostic));return false;
}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6X509PathOutcomeHandlePolicyV1Get,xl6X509PathOutcomeDestroyV1)
