#include "src/common/xl6_xrt.h"
#include "xl6_cell_native_graph.h"
#include "src/api/xl_foreign_error.h"
static bool valid(const void* raw,size_t bytes)
{
    const XL6_NativeGraphCaptureProfileV1* p=xl6NativeGraphCaptureProfileV1Get(raw);
    return p && p->bytes==bytes;
}
static bool destroy(const void* raw,void* value)
{ return ((const XL6_NativeGraphCaptureProfileV1*)raw)->Detach(value,NULL); }
static bool trace(const void* raw,const void* value,XL6_ModuleOwnershipVisit visit,void* context)
{ return ((const XL6_NativeGraphCaptureProfileV1*)raw)->Trace(value,visit,context); }
XL6_Cell* xl6CellCreateNativeGraphV3(const XL6_NativeCaptureProfileV1* prefix)
{
    static const XL6_CellGraphOpsV1 ops={valid,destroy,trace};
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope))return NULL;
    const XL6_NativeGraphCaptureProfileV1* profile=xl6NativeGraphCaptureProfileV1Get(prefix);
    XL6_Cell* result=NULL;
    if(!profile)xrtSetErrorKind(XERR_ARGUMENT);
    else result=xl6CellCreateGraphV1(profile->bytes,&ops,profile);
    if(!result || xrtGetError() || scope.failed)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok){xl6CellRuntimeV1Get()->Release(result);xrtSetErrorTake(outcome.error);return NULL;}
    return result;
}
