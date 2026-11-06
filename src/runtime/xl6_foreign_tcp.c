#include "xl6_foreign_resource.h"
#include "xl6_native_class_borrow.h"
#include "xl6_tcp_internal.h"

static void* exact(const xvalue* value,const XL6_NativeHandlePolicyV1* expected)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!value||!xrtValueGetHandle(value,&raw,&ops,&user)||!raw||user||ops!=expected->Ops){
        xrtSetErrorKind(XERR_TYPE);return NULL;}
    return raw;
}

xvalue* xl6ForeignTcpOwnerV1(XL6_NativeClassFieldContextV1 context,const xvalue* owner)
{
    void* raw=NULL;
    return xl6NativeClassFieldRetainV1(context,owner,xl6TcpStreamHandlePolicyV1Get(),&raw);
}

bool xl6ForeignTcpVisitV1(const xvalue* owner,XL6_ForeignTcpVisitV1 visit,void* data)
{
    if(!visit){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    XL6_ExecutionResourceV1* resource=exact(owner,xl6TcpStreamHandlePolicyV1Get());
    if(!resource)return false;
    xvalue* pin=xrtValueRetain((xvalue*)owner);
    if(!pin)return false;
    XL6_TcpScopeV1 scope={0};
    bool entered=xl6ExecutionTcpEnterInternalV1(resource,XL6_EXECUTION_TCP_STREAM_V1,true,&scope);
    bool ok=entered && visit(scope.stream,data);
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,!ok))abort();
    if(entered)xl6ExecutionTcpLeaveInternalV1(&scope);
    xrtValueRelease(pin);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return ok&&outcome.ok;
}
