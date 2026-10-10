#include "src/runtime/xl6_value_copy.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_list.h"
#include "src/api/xl_foreign_error.h"

xvalue* xl6ValueCopyV1(const xvalue* source)
{
    xl6_error_scope scope={0};
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return NULL;
    }
    int list=xl6ListErasedProbeV1(source);
    xvalue* result=list<0 ? NULL : list ? xl6ListErasedCloneV1(source) : xl6ValueCloneV1(source);
    if (!result || scope.failed || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (scope.failed) { xrtValueRelease(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
