#include "src/common/xl6_xrt.h"
#include "xl6_closure_capture_v4.h"
#include "src/api/xl_foreign_error.h"

static bool reference_only(xrtownershipref ref,void* context)
{ (void)ref; (void)context; return true; }
static bool valid(const void* raw,const void* environment,size_t bytes)
{
    const XL6_CaptureLayoutV2* layout=raw;
    return layout && layout->count && xl6CaptureRuntimeV3Get()->Trace(layout,environment,bytes,reference_only,NULL);
}
static bool trace(const void* raw,const void* environment,size_t bytes,XL6_ModuleOwnershipVisit visit,void* context)
{
    return !environment || xl6CaptureRuntimeV3Get()->Trace(raw,environment,bytes,visit,context);
}
static bool drop(const void* raw,void* environment,size_t bytes)
{
    if (!environment) return true;
    if (!valid(raw,environment,bytes)) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    xl6_error_result outcome=xl6CaptureRuntimeV3Get()->Cleanup(raw,environment,bytes,(xl6_error_result){true,NULL});
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    xrtFree(environment); return outcome.ok;
}
static XL6_Closure* create(const XL6_CodeOwner* owner,void (*invoke)(void),void (*callback)(void),
    void* environment,size_t bytes,const XL6_CaptureLayoutV2* layout,
    const char* const* names,const xlir_call_default* defaults,size_t count)
{
    static const XL6_ClosureEnvironmentOpsV1 ops={sizeof(ops),valid,trace,drop};
    return xl6ClosureCreateEnvironmentV1(owner,invoke,callback,environment,bytes,layout,&ops,names,defaults,count);
}
static bool drop_environment(const XL6_CaptureLayoutV2* layout,void* environment,size_t bytes)
{ return drop(layout,environment,bytes); }
const XL6_ClosureRuntimeV3* xl6ClosureRuntimeV4Get(void)
{ static const XL6_ClosureRuntimeV3 api={create,drop_environment}; return &api; }
