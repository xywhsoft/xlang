#include "xl6_execution_tls_internal.h"

typedef XL6_ExecutionResourceV1 Resource;
typedef XL6_TlsStreamResourceV1 Stream;
Resource* xl6ExecutionTlsPrepareAcceptV1(XL6_ExecutionSourceV1* source,Resource* receiver)
{
    XL6_TlsScopeV1 scope={0};if(!xl6ExecutionTlsListenerEnterV1(receiver,true,&scope))return NULL;
    const XL6_TlsStreamCreateOptionsV1 tls={.engine=scope.engineOwner,.accepting=receiver};
    const XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_TLS_STREAM_V1,.deferRollback=true,.value.tlsStream=&tls};
    Resource* result=xl6ExecutionCreateInternalV1(source,xl6ExecutionTlsStreamFamilyV1Get(),&options);
    xl6ExecutionTlsLeaveV1(&scope);return result;
}
xfuture* xl6ExecutionTlsAcceptSourceForV1(Resource* base,Resource* expected)
{
    if(!expected){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceControlV1(base,true,&scope))return NULL;
    xfuture* result=NULL;
    if(base->family!=xl6ExecutionTlsStreamFamilyV1Get())xrtSetErrorKind(XERR_TYPE);
    else{
        Stream* r=(Stream*)base;xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
        if(r->accepting!=expected)xrtSetErrorKind(XERR_STATE);
        else{
            if(!pending&&xrtAtomic32Load(&r->prepared,XMEMORY_ACQUIRE)){
                XL6_TlsScopeV1 accepting={0};
                if(xl6ExecutionTlsListenerEnterV1(expected,true,&accepting)){
                    pending=xrtTlsListenerAcceptAsync(accepting.listener);
                    if(pending){
                        xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
                        xrtAtomicPtrStore(&r->pending,pending,XMEMORY_RELEASE);xrtAtomic32Store(&r->prepared,0,XMEMORY_RELEASE);
                        if(!xrtOwnershipScopeEnd(&mutation))abort();
                    }
                    xl6ExecutionTlsLeaveV1(&accepting);
                }
            }
            if(pending)result=xrtFutureRef(pending);else if(!xrtGetError())xrtSetErrorKind(XERR_STATE);
        }
    }
    xl6ExecutionResourceLeaveV1(&scope);return result;
}
bool xl6ExecutionTlsPromoteAcceptV1(Resource* base)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceControlV1(base,true,&scope))return false;
    bool ok=false;
    if(base->family!=xl6ExecutionTlsStreamFamilyV1Get())xrtSetErrorKind(XERR_TYPE);
    else{
        Stream* r=(Stream*)base;xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
        if(!r->accepting||!pending||xrtAtomicPtrLoad(&r->native,XMEMORY_ACQUIRE)||
            xrtFutureState(pending)!=XFUTURE_RESOLVED)xrtSetErrorKind(XERR_STATE);
        else{
            xtlsstream* value=xrtFutureValue(pending);xtlsstream* owned=value?xrtTlsStreamRef(value):NULL;
            if(!value)xrtSetErrorKind(XERR_STATE);
            if(owned){
                xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
                xrtAtomicPtrStore(&r->native,owned,XMEMORY_RELEASE);xrtAtomicPtrStore(&r->pending,NULL,XMEMORY_RELEASE);
                if(!xrtOwnershipScopeEnd(&mutation))abort();
                xrtFutureDestroy(pending);xl6ExecutionTlsParentDropV1(&r->accepting);ok=true;
            }
        }
    }
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
