/* Physical shared owner for explicitly declared native compositions. The
 * resident identities below admit only records constructed by this API. */
typedef struct ForeignGraph {
    xatomic64 refs;
    xatomic32 active;
    const void* claim;
    bool cleared,prepared;
    void* raw;
    xvalue* releaser;
    xvalue* argument;
    XL6_ForeignGraphDescriptorV1 descriptor;
} ForeignGraph;

static void graphBegin(xrtownershipscope* scope)
{ if(!xrtOwnershipMutationBegin(scope))abort(); }
static void graphEnd(xrtownershipscope* scope)
{ if(!xrtOwnershipScopeEnd(scope))abort(); }
static bool graphInvoke(ForeignGraph* graph)
{
    xrtownershipscope mutation={0};graphBegin(&mutation);
    bool needed=graph->raw!=NULL;
    graph->raw=NULL;graphEnd(&mutation);
    if(!needed)return true;
    xl6_error_scope scope={0};if(!xl6ErrorScopeBegin(&scope))abort();
    xvalue* arguments[]={graph->argument};
    xrtcallframe frame={.ArgumentCount=1,.Arguments=arguments};
    xrtcallresult result=XRT_CALL_RESULT_INIT;
    bool ok=xrtValueInvoke(graph->releaser,&frame,&result);
    if(ok&&xrtCallResultCount(&result)){xrtSetErrorKind(XERR_TYPE);ok=false;}
    if(!ok||xrtGetError())xl6ErrorScopeCaptureFailure(&scope);
    xrtCallResultUnit(&result);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);
    return outcome.ok;
}
static bool graphHold(const void* raw)
{
    ForeignGraph* graph=(ForeignGraph*)raw;
    if(!graph)return false;
    uint64 refs=xrtAtomic64Load(&graph->refs,XMEMORY_RELAXED);
    do { if(!refs||refs>=SIZE_MAX)return false; }
    while(!xrtAtomic64CompareExchange(&graph->refs,&refs,refs+1,XMEMORY_RELAXED,XMEMORY_RELAXED));
    return true;
}
void xl6ForeignGraphDestroyV1(void* raw)
{
    ForeignGraph* graph=raw;if(!graph)return;
    xrtownershipscope scope={0};graphBegin(&scope);
    uint64 refs=xrtAtomic64FetchSub(&graph->refs,1,XMEMORY_ACQ_REL);
    if(!refs)abort();
    bool last=refs==1;graphEnd(&scope);
    if(!last)return;
    if(xrtAtomic32Load(&graph->active,XMEMORY_ACQUIRE))abort(); /* Each active scope owns a reference. */
    xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,false))abort();
    (void)graphInvoke(graph);
    xrtValueRelease(graph->argument);xrtValueRelease(graph->releaser);xrtFree(graph);
    xl6_error_result result=xl6ErrorScopeEnd(&cleanup);
    if(!result.ok)xl6ErrorScopePropagateFailure(result);
}
static void graphDrop(const void* raw) { xl6ForeignGraphDestroyV1((void*)raw); }
static bool graphCount(const void* raw,size_t* count)
{
    const ForeignGraph* graph=raw;
    if(!graph||!count||graph->cleared||xrtAtomic32Load(&graph->active,XMEMORY_ACQUIRE))return false;
    uint64 refs=xrtAtomic64Load(&graph->refs,XMEMORY_ACQUIRE);
    if(!refs||refs>SIZE_MAX)return false;
    *count=(size_t)refs;return true;
}
static bool graphTrace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const ForeignGraph* graph=raw;size_t count;
    return visit&&graphCount(raw,&count)&&
        visit(xrtValueOwnership(graph->releaser),context)&&
        visit(xrtValueOwnership(graph->argument),context)&&
        (!graph->raw||graph->descriptor.Trace(graph->raw,visit,context));
}
static const xrtownershipops graphOps={graphCount,graphTrace};
static bool graphBusy(const void* raw)
{ return xrtAtomic32Load(&((const ForeignGraph*)raw)->active,XMEMORY_ACQUIRE)!=0; }
static bool graphClaim(const void* raw,const void* token)
{
    ForeignGraph* graph=(ForeignGraph*)raw;
    if(!token||graph->cleared||graphBusy(graph)||(graph->claim&&graph->claim!=token))return false;
    graph->claim=token;return true;
}
static void graphRestore(const void* raw,const void* token)
{ ForeignGraph* graph=(ForeignGraph*)raw;if(graph->claim==token&&!graph->cleared)graph->claim=NULL; }
static bool graphReady(const void* raw)
{ const ForeignGraph* graph=raw;return !graphBusy(graph)&&(!graph->raw||graph->prepared); }
static xrtownershipprepareresult graphPrepare(const void* raw,const void* token)
{
    ForeignGraph* graph=(ForeignGraph*)raw;
    if(!token||graph->claim!=token||graph->cleared)return XRT_OWNERSHIP_PREPARE_FAILED;
    if(graphReady(graph))return XRT_OWNERSHIP_PREPARE_READY;
    xrtownershipprepareresult result=graph->descriptor.Prepare
        ? graph->descriptor.Prepare(graph->raw) : XRT_OWNERSHIP_PREPARE_READY;
    if(result==XRT_OWNERSHIP_PREPARE_READY){
        xrtownershipscope scope={0};graphBegin(&scope);graph->prepared=true;graphEnd(&scope);
    }
    return result;
}
static bool graphFinalize(const void* raw,const void* token)
{
    ForeignGraph* graph=(ForeignGraph*)raw;
    if(!token||graph->claim!=token||!graphReady(graph)||graph->cleared)return false;
    return graphInvoke(graph);
}
static void graphClear(const void* raw,const void* token)
{
    ForeignGraph* graph=(ForeignGraph*)raw;
    if(!token||graph->claim!=token||graph->raw||graphBusy(graph))abort();
    graph->cleared=true;
}
static bool graphFinish(const void* raw,const void* token)
{
    const ForeignGraph* graph=raw;
    /* Captures/code remain real retained edges until the last physical pin.
     * Their own Clear cannot unmap callback code on a generated stack. */
    return token&&graph->claim==token&&graph->cleared&&!graph->raw;
}
static const xrtownershipadapterv1 graphAdapter={sizeof(graphAdapter),graphHold,graphDrop,
    graphClaim,graphRestore,graphFinalize,graphClear,graphFinish};
static const xrtownershippreparationv1 graphPreparation={sizeof(graphPreparation),
    &graphAdapter,graphReady,graphPrepare};
static const xrtownershipquiescencev1 graphQuiescence={sizeof(graphQuiescence),&graphOps,graphBusy};
static bool graphHandleTrace(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;const xvaluehandleops* ops=NULL;void* user=NULL;
    const XL6_NativeHandlePolicyV1* policy=xl6ForeignGraphHandlePolicyV1Get();
    return visit&&xrtValueGetHandle(value,&raw,&ops,&user)&&raw&&!user&&ops==policy->Ops&&
        visit((xrtownershipref){raw,&graphOps},context);
}
static void graphHandleDrop(void* raw,void* user)
{ if(user)abort();xl6ForeignGraphDestroyV1(raw); }
const XL6_NativeHandlePolicyV1* xl6ForeignGraphHandlePolicyV1Get(void)
{
    static const xvaluehandleops ops={NULL,graphHandleDrop,NULL,NULL};
    static const XL6_NativeHandlePolicyV1 policy={sizeof(policy),XL6_NATIVE_HANDLE_POLICY_V1_TAG,&ops,graphHandleTrace};
    return &policy;
}
static bool foreignGraphLoad(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddHandleV1(catalog,xl6ForeignGraphHandlePolicyV1Get())&&
        xl6OwnershipCatalogAddQuiescenceV1(catalog,&graphQuiescence)&&
        xl6OwnershipCatalogAddNetTaskV1(catalog,xl6ForeignGraphTaskPolicyV1Get());
}
static void graphTask(xnetworker* worker,void* raw)
{
    ForeignGraph* graph=raw;
    XL6_ForeignGraphScopeV1 scope={0};
    if(!xl6ForeignGraphEnterV1(graph,&scope))return;
    if(graph->descriptor.Task)graph->descriptor.Task(worker,scope.raw);
    else xrtSetErrorKind(XERR_ARGUMENT);
    /* The native Task's transferred reference still protects the descriptor
     * and package code after Leave, until resident Task Drop runs. */
    xl6ForeignGraphLeaveV1(&scope);
}
const xnettaskownershipv1* xl6ForeignGraphTaskPolicyV1Get(void)
{
    static const xnettaskownershipv1 policy={sizeof(policy),graphTask,xl6ForeignGraphDestroyV1,&graphOps};
    return &policy;
}
static const xrtownershipadapterv1* foreignGraphResolve(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{
    if(ref.Ops!=&graphOps||!ref.Data)return NULL;
    *preparation=&graphPreparation;return &graphAdapter;
}
void* xl6ForeignGraphCreateV1(void* raw,const XL6_ForeignGraphDescriptorV1* descriptor,const xvalue* releaser)
{
    /* A language-owned point action certifies module/dependency residency.
     * An arbitrary native callable's context is not a code capability. */
    if(!raw||!descriptor||descriptor->size!=sizeof(*descriptor)||
       descriptor->abiTag!=XL6_FOREIGN_GRAPH_V1_TAG||!descriptor->Trace||
       !xl6ClosureBoxIsPointActionV1(releaser)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;
    }
    ForeignGraph* graph=xrtCalloc(1,sizeof(*graph));if(!graph)return NULL;
    xrtAtomic64Init(&graph->refs,1);xrtAtomic32Init(&graph->active,0);graph->descriptor=*descriptor;
    graph->argument=xrtValuePointer(raw);graph->releaser=xrtValueRetain((xvalue*)releaser);
    if(!graph->argument||!graph->releaser){xl6ForeignGraphDestroyV1(graph);return NULL;}
    graph->raw=raw;return graph;
}
bool xl6ForeignGraphEnterV1(void* raw,XL6_ForeignGraphScopeV1* scope)
{
    ForeignGraph* graph=raw;
    if(!scope||scope->record||scope->raw||!graph){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};graphBegin(&mutation);
    uint32 inactive=0;
    bool ok=!graph->claim&&!graph->cleared&&!graph->prepared&&graph->raw&&
        xrtAtomic32CompareExchange(&graph->active,&inactive,1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE);
    if(ok){
        ok=graphHold(graph);
        if(ok){scope->raw=graph->raw;scope->record=graph;}
        else xrtAtomic32Store(&graph->active,0,XMEMORY_RELEASE);
    }
    graphEnd(&mutation);if(!ok)xrtSetErrorKind(XERR_STATE);return ok;
}
void xl6ForeignGraphLeaveV1(XL6_ForeignGraphScopeV1* scope)
{
    if(!scope||!scope->record)return;
    ForeignGraph* graph=scope->record;
    xrtownershipscope mutation={0};graphBegin(&mutation);
    if(xrtAtomic32Load(&graph->active,XMEMORY_ACQUIRE)!=1)abort();
    xrtAtomic32Store(&graph->active,0,XMEMORY_RELEASE);graphEnd(&mutation);
    scope->record=NULL;scope->raw=NULL;xl6ForeignGraphDestroyV1(graph);
}
