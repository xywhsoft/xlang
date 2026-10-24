#include "xl6_execution_tls_internal.h"
typedef XL6_ExecutionResourceV1 Resource;
typedef XL6_TlsStreamResourceV1 Stream;
/* Existing accepted work may finish after logical Engine Close; this is an
 * identity/lifetime read, not permission to start a new Engine activation. */
bool xl6ExecutionTlsPromotePendingV1(Resource* base,XL6_TlsPendingKindV1 kind)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceControlV1(base,true,&scope))return false;
    bool ok=false;
    if(base->family!=xl6ExecutionTlsStreamFamilyV1Get())xrtSetErrorKind(XERR_TYPE);
    else{
        Stream* r=(Stream*)base;xfuture* pending=xrtAtomicPtrLoad(&r->pending,XMEMORY_ACQUIRE);
        if(kind==XL6_TLS_PENDING_NONE_V1||r->pendingKind!=kind||!pending||xrtAtomicPtrLoad(&r->native,XMEMORY_ACQUIRE)||
            xrtFutureState(pending)!=XFUTURE_RESOLVED)xrtSetErrorKind(XERR_STATE);
        else{
            xtlsstream* value=xrtFutureValue(pending);xtlsstream* owned=NULL;
            XL6_ExecutionScopeV1 engine={0};
            if(!value)xrtSetErrorKind(XERR_STATE);
            else if(xl6ExecutionResourceEnterV1(r->parent,false,&engine)){
                xnetstream* tcp=xrtTlsStreamTransport(value);xnetworker* worker=tcp?xrtNetStreamWorker(tcp):NULL;
                if(r->parent->family!=xl6ExecutionNetEngineFamilyV1Get()||!engine.engine||!worker||xrtNetWorkerEngine(worker)!=engine.engine)xrtSetErrorKind(XERR_TYPE);
                else owned=xrtTlsStreamRef(value);
                xl6ExecutionResourceLeaveV1(&engine);
            }
            if(owned){
                xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
                xrtAtomicPtrStore(&r->native,owned,XMEMORY_RELEASE);xrtAtomicPtrStore(&r->pending,NULL,XMEMORY_RELEASE);
                if(!xrtOwnershipScopeEnd(&mutation))abort();
                xrtFutureDestroy(pending);xl6ExecutionTlsParentDropV1(&r->accepting);
                xl6ExecutionTlsParentDropV1(&r->resolving);ok=true;
            }
        }
    }
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}
