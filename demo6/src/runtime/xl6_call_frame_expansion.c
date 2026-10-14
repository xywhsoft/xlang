#include "src/runtime/xl6_call_frame_expansion_internal.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

bool xl6CallFrameExpansionFinishV1(void** input, bool failedCall)
{
    xl6_error_scope cleanup={0};
    if (!input) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_FrameExpansion* owner=*input; *input=NULL;
    if (!xl6ErrorScopeBeginCleanup(&cleanup,failedCall)) abort();
    if (owner) {
        for (size_t i=owner->argumentCount; i>0; --i) {
            xvalue* value=owner->arguments[i-1]; owner->arguments[i-1]=NULL;
            xrtValueRelease(value); if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);
        }
        for (size_t i=owner->keywordCount; i>0; --i) {
            xvalue* value=owner->keywords[i-1]; owner->keywords[i-1]=NULL;
            xrtFree((void*)owner->names[i-1].Data); owner->names[i-1]=(xstrview){0};
            xrtValueRelease(value); if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);
        }
        xrtValueIterEnd(&owner->iterator); if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);
        xrtValueRelease(owner->snapshot); if (xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);
        xrtFree(owner->arguments); xrtFree(owner->keywords); xrtFree(owner->names); xrtFree(owner);
    }
    xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
    xrtSetErrorTake(result.error); return result.ok;
}
