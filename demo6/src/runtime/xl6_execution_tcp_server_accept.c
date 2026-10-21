#include "xl6_execution_tcp_internal.h"
#include "xl6_execution_tcp_server_internal.h"
#include <stdlib.h>
typedef XL6_ExecutionResourceV1 Resource;
Resource* xl6ExecutionTcpServerPrepareAcceptV1(XL6_ExecutionSourceV1* source,Resource* receiver)
{
    XL6_TcpServerScopeV1 scope={0};if(!xl6ExecutionTcpServerEnterInternalV1(receiver,true,&scope))return NULL;
    XL6_TcpCreateOptionsV1 tcp={.engine=scope.engineOwner,.accepting=receiver,.serverAccept=true};
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TCP_STREAM_V1,.value.tcp=&tcp};
    Resource* result=xl6ExecutionCreateInternalV1(source,xl6ExecutionTcpStreamFamilyV1Get(),&options);
    xl6ExecutionTcpServerLeaveInternalV1(&scope);return result;
}
xfuture* xl6ExecutionTcpServerAcceptSourceForV1(Resource* base,Resource* expected)
{
    if(!expected){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceControlV1(base,true,&scope))return NULL;
    xfuture* result=NULL;
    if(base->family!=xl6ExecutionTcpStreamFamilyV1Get())xrtSetErrorKind(XERR_TYPE);
    else{
        XL6_TcpStreamResourceV1* r=(XL6_TcpStreamResourceV1*)base;
        xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
        if(r->pendingKind!=XL6_TCP_PENDING_SERVER_ACCEPT_V1||r->accepting!=expected)xrtSetErrorKind(XERR_STATE);
        else{
            if(!pending&&xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE)){
                XL6_TcpServerScopeV1 accepting={0};
                if(xl6ExecutionTcpServerEnterInternalV1(r->accepting,true,&accepting)){
                    pending=xrtNetServerAcceptAsync(accepting.server);
                    if(pending){xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
                        xrtAtomicPtrStore(&r->pending,pending,XMEMORY_RELEASE);xrtAtomic32Store(&r->prepared,0,XMEMORY_RELEASE);
                        if(!xrtOwnershipScopeEnd(&mutation))abort();}
                    xl6ExecutionTcpServerLeaveInternalV1(&accepting);
                }
            }
            if(pending)result=xrtFutureRef(pending);else if(!xrtGetError())xrtSetErrorKind(XERR_STATE);
        }
    }
    xl6ExecutionResourceLeaveV1(&scope);return result;
}
bool xl6ExecutionTcpServerPromoteAcceptV1(Resource* carrier)
{return xl6ExecutionTcpPromotePendingInternalV1(carrier,XL6_TCP_PENDING_SERVER_ACCEPT_V1);}
