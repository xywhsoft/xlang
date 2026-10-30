/* Actual native handler implementation, part of the resident API TU. Every
 * registry read/transition uses the host API boundary. The registry borrows
 * identities; neither it nor discovery contributes a reference count. */
typedef struct XL6_XlangHandler {
    XL_Context* context;
    XL_Module* module;
    xmutex* gate;
    uint64_t runningThread;
    struct XL6_XlangHandler* previous;
    struct XL6_XlangHandler* next;
} XL6_XlangHandler;
static XL6_XlangHandler* xl__handlers;
static bool xl__resolvingHandlerCatalog;
static bool xlHandlerKnown(const XL6_XlangHandler* handler)
{
    for(const XL6_XlangHandler* at=xl__handlers;at;at=at->next)if(at==handler)return true;
    return false;
}
static const char* xlHandlerString(xstrview text)
{
    if(!text.Size)return "";
    if(!text.Data){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    if(memchr(text.Data,0,text.Size)){xrtSetErrorKind(XERR_VALUE);return NULL;}
    return text.Data;
}
void* xl6_xlang_handler_wrap(void* value){return value;}
void* xl6_xlang_handler_create(xstrview pathValue,xstrview workValue)
{
    const char* path=xlHandlerString(pathValue);
    const char* work=path?xlHandlerString(workValue):NULL;
    if(!path||!work)return NULL;
    if(!path[0]){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_XlangHandler* handler=xrtCalloc(1,sizeof(*handler));
    if(!handler)return NULL;
    handler->gate=xrtMutexCreate();handler->context=xlCreateContext(NULL);
    if(!handler->gate||!handler->context)goto failed;
    if(work[0]&&!xlSetWorkDir(handler->context,work))goto failed;
    handler->module=xlCompileScriptFileToModule(handler->context,path);
    if(!handler->module)goto failed;
    xlApiLock();handler->next=xl__handlers;
    if(xl__handlers)xl__handlers->previous=handler;
    xl__handlers=handler;xlApiUnlock();
    return handler;
failed: {
    xerror* failure=xrtTakeError();
    xlDestroyContext(handler->context);
    if(handler->gate)(void)xrtMutexDestroy(handler->gate);
    xrtFree(handler);xrtSetErrorTake(failure);return NULL;
    }
}
void xl6_xlang_handler_destroy(void* raw)
{
    XL6_XlangHandler* handler=raw;
    if(!handler)return;
    xlApiLock();
    if(!xlHandlerKnown(handler)||handler->runningThread){xlApiUnlock();xrtSetErrorKind(XERR_STATE);return;}
    if(handler->previous)handler->previous->next=handler->next;else xl__handlers=handler->next;
    if(handler->next)handler->next->previous=handler->previous;
    xlApiUnlock();
    xlDestroyModule(handler->module);xlDestroyContext(handler->context);
    if(!xrtMutexDestroy(handler->gate))abort();
    xrtFree(handler);
}
static bool xlHandlerEnter(XL6_XlangHandler* handler)
{
    xlApiLock();bool known=xlHandlerKnown(handler);
    bool self=known&&handler->runningThread==xrtThreadCurrentId();xlApiUnlock();
    if(!known||self){xrtSetErrorKind(known?XERR_STATE:XERR_ARGUMENT);return false;}
    if(!xrtMutexLock(handler->gate))return false;
    xlApiLock();handler->runningThread=xrtThreadCurrentId();xlApiUnlock();return true;
}
static void xlHandlerLeave(XL6_XlangHandler* handler)
{
    xlApiLock();handler->runningThread=0;xlApiUnlock();
    if(!xrtMutexUnlock(handler->gate))abort();
}
bool xl6_xlang_handler_run_args(void* raw,xvalue* args)
{
    XL6_XlangHandler* handler=raw;
    if(!xlHandlerEnter(handler))return false;
    bool ok=xlRunModuleMain(handler->context,handler->module,args)!=0;
    xlHandlerLeave(handler);return ok;
}
void* xl6_xlang_handler_last_error_copy(void* raw)
{
    XL6_XlangHandler* handler=raw;
    if(!xlHandlerEnter(handler))return NULL;
    const char* message=xlGetLastError(handler->context);
    char* result=xrtStrDup(message?message:"");xlHandlerLeave(handler);return result;
}
bool xl6XlangHandlerTraceV1(const xvalue* value,xrtownershipvisitor visit,void* context)
{
    void* raw=NULL;void* user=NULL;const xvaluehandleops* ops=NULL;
    if(!visit||!xrtValueGetHandle(value,&raw,&ops,&user)||user||
       ops!=xl6XlangHandlerHandlePolicyV1Get()->Ops||!xlHandlerKnown(raw))return false;
    const XL6_XlangHandler* handler=raw;
    /* A graphless verified artifact owns compiler/native state only. A graph
     * artifact contributes its ONE real caller handle to the module node. */
    return !handler->module->ownershipTrace||
        visit((xrtownershipref){handler->module,&xlModuleOwnershipOps},context);
}
static bool xlHandlerModuleBusy(const void* raw)
{
    const XL_Module* module=raw;
    return module&&(module->retirementRunning||module->activeCalls||
        xl6CodeOwnerScopeActive(&module->codeOwnerGraph.base)||
        module->activation==XL_MODULE_DEACTIVATING||
        (module->activation==XL_MODULE_DEACTIVATION_PENDING&&!module->retirementPrepared));
}
const xrtownershipquiescencev1* xl6XlangModuleQuiescenceV1Get(void)
{
    static const xrtownershipquiescencev1 policy={sizeof(policy),&xlModuleOwnershipOps,xlHandlerModuleBusy};
    return &policy;
}
static bool xlHandlerDiscoverModule(const XL_Module* module,xrtownershipvisitor visit,void* context)
{
    size_t count;
    if(!module||!module->retirementContract||module->ownershipCleared||xlHandlerModuleBusy(module)||
       !xlModuleOwnershipCount(module,&count)||!count)return true;
    return visit((xrtownershipref){module,&xlModuleOwnershipOps},context)&&
        xl6ClassOwnershipDiscoverV1(&module->codeOwnerGraph.base,visit,context);
}
static bool xlHandlerBoundaryDiscover(xrtownershipvisitor visit,void* context,void* unused)
{
    (void)unused;
    for(const XL6_XlangHandler* handler=xl__handlers;handler;handler=handler->next)
        if(!xlHandlerDiscoverModule(handler->module,visit,context))return false;
    for(const XL_Module* module=xl__retiringModules;module;module=module->retirementNext)
        if(!xlHandlerDiscoverModule(module,visit,context))return false;
    return true;
}
const XL6_OwnershipBoundaryV1* xl6XlangHandlerBoundaryV1Get(void)
{
    static const XL6_OwnershipBoundaryV1 boundary={sizeof(boundary),xlModuleBoundaryEnter,
        xlModuleBoundaryLeave,xlHandlerBoundaryDiscover,NULL};return &boundary;
}
bool xl6XlangHandlerResolveV1(xrtownershipref ref,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    *adapter=NULL;*preparation=NULL;*completion=NULL;
    if(ref.Ops==&xlModuleOwnershipOps)
        return xlModuleResolveOwnershipV3(ref,NULL,adapter,preparation,completion);
    if(xl__resolvingHandlerCatalog)return false;
    xl__resolvingHandlerCatalog=true;bool found=false;
    for(const XL6_XlangHandler* handler=xl__handlers;handler&&!found;handler=handler->next)
        if(handler->module->ownershipCatalog)
            found=xl6OwnershipCatalogResolveV1(ref,handler->module->ownershipCatalog,adapter,preparation,completion);
    for(const XL_Module* module=xl__retiringModules;module&&!found;module=module->retirementNext)
        if(module->ownershipCatalog)
            found=xl6OwnershipCatalogResolveV1(ref,module->ownershipCatalog,adapter,preparation,completion);
    xl__resolvingHandlerCatalog=false;return found;
}
