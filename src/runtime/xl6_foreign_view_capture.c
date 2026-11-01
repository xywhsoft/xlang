#include "src/common/xl6_xrt.h"
#include "xl6_foreign_view_capture.h"
#include <stdlib.h>

static bool copy_fresh(void* raw,const void* input)
{
    XL6_ForeignView** output=raw;
    if (!output || !input || *output) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_ForeignView* value=*(XL6_ForeignView* const*)input;
    XL6_ForeignView* copy=xl6ForeignViewRetainV1(value);
    if (value && !copy) return false;
    *output=copy; return true;
}
static bool trace_slot(const void* raw,XL6_ModuleOwnershipVisit visit,void* context)
{
    const XL6_ForeignView* value=*(XL6_ForeignView* const*)raw;
    return !value || visit(xl6ForeignViewOwnershipV1(value),context);
}
static bool detach(void* raw,unsigned char* live)
{
    XL6_ForeignView** slot=raw;
    xrtownershipscope mutation={0}; if (!xrtOwnershipMutationBegin(&mutation)) abort();
    XL6_ForeignView* owned=*slot; *slot=NULL; if (live) *live=0;
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    return xl6ForeignViewReleaseV1(owned);
}
const XL6_NativeCaptureProfileV1* xl6ForeignViewCaptureProfileV1Get(void)
{
    static const XL6_NativeGraphCaptureProfileV1 profile={
        {sizeof(profile),UINT64_C(0x584C364656435031),NULL,NULL},XL6_NATIVE_GRAPH_CAPTURE_V1_TAG,
        sizeof(XL6_ForeignView*),_Alignof(XL6_ForeignView*),copy_fresh,trace_slot,detach};
    return &profile.prefix;
}
