/* A mixed generated/native module. This native Promise slot really lives in
 * the TCC data segment and owns exactly the endpoint created below. Compose
 * its Trace/Finish with the generated contract; no synthetic node, extra code
 * lease or host-held Promise is introduced for the rootless modes. */
#define xl_script_module_ownership_trace pending_generated_trace
#define xl_script_module_ownership_read_v1 pending_generated_read_v1
#define xl_script_module_retirement_v1 pending_generated_graph
#define xl_script_module_retirement_v4 pending_generated_retirement_v4
#define xl_script_module_retire_step pending_generated_retire_step
#define xl_script_module_unit_checked pending_generated_unit_checked
#define xl_script_module_unit pending_generated_unit
#include "xlir_pending_module_preparation.c"
#undef xl_script_module_ownership_trace
#undef xl_script_module_ownership_read_v1
#undef xl_script_module_retirement_v1
#undef xl_script_module_retirement_v4
#undef xl_script_module_retire_step
#undef xl_script_module_unit_checked
#undef xl_script_module_unit

typedef struct PendingModuleHooks {
    void (*observe)(int64_t,const char*);
    void (*capture)(xvalue*);
} PendingModuleHooks;
static PendingModuleHooks pending_hooks;
static xpromise* pending_source;
void pending_module_bind(const PendingModuleHooks* hooks) { pending_hooks=*hooks; }
void pending_module_observe(int64_t event,const char* text) { pending_hooks.observe(event,text); }
void pending_module_capture(xvalue* value) { pending_hooks.capture(value); }
xfuture* pending_module_input(void)
{
    xfuture* result=NULL;xrtownershipscope mutation={0};
    if(pending_source || !xrtOwnershipMutationBegin(&mutation))abort();
    pending_source=xrtPromiseCreate(&result,NULL);
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    return result;
}
xpromise* pending_module_source_ref(void) { return xrtPromiseRef(pending_source); }
bool xl_script_module_ownership_trace(XL6_ModuleOwnershipVisit visit,void* context)
{
    return pending_generated_trace(visit,context)&&visit(xrtPromiseOwnership(pending_source),context);
}
const XL6_ModuleReadContractV1* xl_script_module_ownership_read_v1(void)
{
    static const XL6_ModuleReadContractV1 read={sizeof(read),0x584C364D524431ULL,xl_script_module_ownership_trace};
    return &read;
}
static void pending_release_source(void)
{
    xrtownershipscope mutation={0};xpromise* source;
    if(!xrtOwnershipMutationBegin(&mutation))abort();
    source=pending_source;pending_source=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    /* Normal deactivation closes before user fields disappear; graph Finish
     * instead reaches this after its certified source has already notified. */
    xrtPromiseDestroy(source);
}
int xl_script_module_unit_checked(void)
{
    pending_release_source();
    return pending_generated_unit_checked();
}
void xl_script_module_unit(void)
{ if(!xl_script_module_unit_checked())xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); }
static const XL6_ModuleRetirementV1 pending_graph={sizeof(pending_graph),0x584C364D525431ULL,
    xl_script_module_ownership_trace,xl_script_module_unit_checked};
const XL6_ModuleRetirementV1* xl_script_module_retirement_v1(void) { return &pending_graph; }
static XL6_ModulePrepareResult pending_prepare(void)
{ return pending_generated_retirement_v4()->preflight(); }
static XL6_ModuleRetireResult pending_step(void)
{
    pending_release_source();
    return pending_generated_retirement_v4()->step();
}
const XL6_ModuleRetirementV4* xl_script_module_retirement_v4(void)
{
    static const XL6_ModuleRetirementV4 preparation={sizeof(preparation),0x584C364D525434ULL,&pending_graph,pending_prepare,pending_step};
    return &preparation;
}
