#include "xl6_cell_native.h"
#include "xl6_text_value.h"
#include "src/api/xl_foreign_error.h"
static bool valid(const void* raw,size_t size)
{
    const XL6_NativeCaptureProfileV1* profile=raw;
    return xl6NativeCaptureProfileValidV1(profile) && profile->Type()->Size==size;
}
static void destroy(const void* raw,void* value)
{((const XL6_NativeCaptureProfileV1*)raw)->DetachCleanup(value,NULL);}
XL6_Cell* xl6CellCreateNativeV2(const XL6_NativeCaptureProfileV1* profile)
{
    static const XL6_CellLeafOpsV1 ops={valid,destroy};
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope)){
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return NULL;
    }
    XL6_Cell* result=NULL;
    if(!xl6NativeCaptureProfileValidV1(profile))xrtSetErrorKind(XERR_ARGUMENT);
    else result=xl6CellCreateLeafV1(profile->Type()->Size,&ops,profile);
    if(!result || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6CellRuntimeV1Get()->Release(result);xl6ErrorScopePropagateFailure(outcome);return NULL;}
    return result;
}
