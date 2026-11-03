#include "src/common/xl6_xrt.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_native_callback.h"
#include "src/runtime/xl6_foreign_view_capture.h"
#include "src/runtime/xl6_foreign_view_value.h"
#include "src/runtime/xl6_closure_capture_v4.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <xui.h>
#include <xge.h>
#include <xge_host_xrt.h>
#include <xge_host_xrt_contract.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct HostSymbol {const char* name;xge_host_xrt_proc address;} HostSymbol;
static HostSymbol symbols[]={
#define XGE_HOST_XRT_SLOT(name) {#name,NULL},
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
};
static xge_host_xrt_proc resolve(void* user,const char* name)
{
    (void)user;size_t first=0,last=sizeof(symbols)/sizeof(symbols[0]);
    while(first<last){size_t middle=first+(last-first)/2;int order=strcmp(symbols[middle].name,name);
        if(order<0)first=middle+1;else if(order>0)last=middle;else return symbols[middle].address;}
    return NULL;
}
static bool watch_context(void* raw,void* storage,void(*notify)(void*),void* user)
{return xuiContextLifetimeWatch(raw,storage,notify,user)==XUI_OK;}
static bool watch_widget(void* raw,void* storage,void(*notify)(void*),void* user)
{return xuiWidgetLifetimeWatch(raw,storage,notify,user)==XUI_OK;}
static void unwatch(void* storage){xuiLifetimeUnwatch(storage);}
static bool destroy_context(void* raw){xuiDestroy(raw);return true;}
static bool destroy_widget(void* raw){xuiWidgetDestroy(raw);return true;}
static bool begin(void* raw,void* storage){return xuiCallScopeBegin(raw,storage)==XUI_OK;}
static bool end(void* storage){return xuiCallScopeEnd(storage)==XUI_OK;}
static bool busy(const void* raw){return xuiContextOwnershipBusy((xui_context)raw);}
/* Native type hooks visit actual unique payloads as well as event owners. */
static bool trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return xuiContextTraceOwnership((xui_context)raw,visit,user);}
static const XL6_ForeignViewDescriptorV1 rootDescriptor={sizeof(rootDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"xui_context",
    sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_context,unwatch,destroy_context,
    sizeof(xui_call_scope_t),_Alignof(xui_call_scope_t),begin,end,trace,busy};
static const XL6_ForeignViewDescriptorV1 widgetDescriptor={sizeof(widgetDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"xui_widget",
    sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_widget,unwatch,destroy_widget,0,1,NULL,NULL,NULL,NULL};
static const XL6_NativeCallbackRuntimeV1* api;
static unsigned invocations;
enum {READ,REMOVE,CLOSE,FAIL};
typedef struct Environment {XL6_ForeignView* view;int action;} Environment;
static const XL6_CaptureSlotV2 slots[]={
    {{XL6_CAPTURE_NATIVE_GRAPH_V3,offsetof(Environment,view),sizeof(XL6_ForeignView*)},xl6ForeignViewCaptureProfileV1Get},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Environment,action),sizeof(int)},NULL}};
static const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Environment),2,slots};
static int body(void* raw,xui_widget widget,const xui_event_t* event)
{
    Environment* environment=raw;(void)event;++invocations;
    assert(xl6ForeignCallbackActiveV1());
    if(environment->view){
        XL6_ForeignViewCallV1 call={0};xui_call_scope_t native={0};
        assert(xl6ForeignViewCallBeginV1(environment->view,&call,&native,sizeof(native)) && call.raw==widget);
        assert(!xuiWidgetGetChildCount(call.raw) && xl6ForeignViewCallEndV1(&call));
    }
    if(environment->action==REMOVE)assert(xuiWidgetSetEventCallback(widget,NULL,NULL)==XUI_OK);
    if(environment->action==CLOSE)xuiDestroy(xuiWidgetGetContext(widget));
    return environment->action==FAIL?XUI_ERROR_CALLBACK_FAILED:XUI_OK;
}
static int thunk(xui_widget widget,const xui_event_t* event,void* raw)
{
    XL6_NativeCallbackScopeV1 scope={0};xui_call_scope_t native={0};
    if(!api->Enter(raw,991,&scope,&native,sizeof(native)))return XUI_ERROR_CALLBACK_FAILED;
    int status=((int(*)(void*,xui_widget,const xui_event_t*))scope.closure.closure->invoke)
        (scope.closure.closure->environment,widget,event);
    xl6_error_result result=api->Leave(&scope,(xl6_error_result){status==XUI_OK,xrtTakeError()});
    if(!result.ok){xl6ErrorScopePropagateFailure(result);return XUI_ERROR_CALLBACK_FAILED;}
    assert(!result.error);return XUI_OK;
}
static XL6_ForeignView* create(xui_context* raw,xui_widget* widget,XL6_ForeignView** child)
{
    assert(xuiCreate(raw)==XUI_OK);XL6_ForeignView* root=xl6ForeignViewCreateRootV1(*raw,&rootDescriptor,NULL);assert(root);
    XL6_ForeignViewCallV1 call={0};xui_call_scope_t native={0};assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));
    assert(xuiWidgetCreate(*raw,widget)==XUI_OK);*child=xl6ForeignViewBorrowChildV1(*widget,root,&widgetDescriptor,NULL);assert(*child);
    assert(xl6ForeignViewCallEndV1(&call));return root;
}
static void bind(xui_widget widget,XL6_ForeignView* root,XL6_ForeignView* capture,int action)
{
    Environment* environment=xrtCalloc(1,sizeof(*environment));assert(environment);environment->action=action;
    environment->view=xl6ForeignViewRetainV1(capture);assert(!capture || environment->view);
    XL6_Closure* closure=xl6ClosureRuntimeV4Get()->Create(NULL,(void(*)(void))body,NULL,environment,sizeof(*environment),&layout,NULL,NULL,0);assert(closure);
    XL6_NativeCallback* record=api->Create(NULL,closure,root,991);assert(record);xl6ClosureRuntimeV1Get()->Release(closure);
    xui_callback_owner_t owner={api->Ownership(record),api->HoldNative,api->DropNative};
    assert(xuiWidgetSetEventCallbackOwned(widget,thunk,record,&owner)==XUI_OK && api->Release(record));
}
static int dispatch(xui_context raw,xui_widget widget)
{xui_event_t event={0};event.iSize=sizeof(event);event.iType=XUI_EVENT_COMMAND;event.pTarget=widget;return xuiDispatchEvent(raw,&event);}
static void calls(void)
{
    xui_context raw=NULL;xui_widget widget=NULL;XL6_ForeignView* child=NULL;
    XL6_ForeignView* root=create(&raw,&widget,&child);bind(widget,root,child,READ);
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);unsigned count=invocations;
    for(unsigned i=0;i<100;++i)assert(dispatch(raw,widget)==XUI_OK);
    xrtMemDebugSnapshot(&after);assert(invocations-count==100 && before.AllocCount==after.AllocCount &&
        before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    bind(widget,root,child,REMOVE);assert(dispatch(raw,widget)==XUI_OK && dispatch(raw,widget)==XUI_OK);
    bind(widget,root,child,FAIL);xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
    assert(dispatch(raw,widget)==XUI_ERROR_CALLBACK_FAILED);
    xl6_error_result result=xl6ErrorScopeEnd(&errors);assert(!result.ok && !result.error);
    bind(widget,root,child,CLOSE);assert(dispatch(raw,widget)==XUI_OK);
    XL6_ForeignViewCallV1 rejected={0};xui_call_scope_t native={0};assert(!xl6ForeignViewCallBeginV1(child,&rejected,&native,sizeof(native)));
    xrtClearError();assert(xl6ForeignViewReleaseV1(child) && xl6ForeignViewReleaseV1(root) && xl6ForeignViewDrainV1());
}
static bool discover(XL6_ModuleOwnershipVisit visit,void* user,void* catalog)
{(void)catalog;return xl6ForeignViewDiscoverV1(NULL,visit,user);}
static void cycle(void)
{
    xui_context raw=NULL;xui_widget widget=NULL;XL6_ForeignView* child=NULL;
    XL6_ForeignView* root=create(&raw,&widget,&child);bind(widget,root,child,READ);
    assert(xl6ForeignViewReleaseV1(child) && xl6ForeignViewReleaseV1(root));
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),xl6OwnershipDomainNativeCallbackV1Get(),xl6OwnershipDomainClosureV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(xl6ForeignViewOwnershipV1(root),xl6OwnershipCatalogProbeV1,discover,true,catalog,&plan)>0 && plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    XL6_OwnershipStepResult step=XL6_OWNERSHIP_STEP_BUSY;
    for(unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i)step=xl6OwnershipPlanFinalizeV2(plan);
    assert(step==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanTryDestroy(plan)>0);
    xl6OwnershipCatalogDestroyV1(catalog);assert(xl6ForeignViewDrainV1());
}
static bool count_edge(xrtownershipref reference,void* raw)
{assert(reference.Data && reference.Ops);++*(unsigned*)raw;return true;}
static void payload_cycles(void)
{
    /* Two graphs, their current configs/history, schema/default containers,
     * and an owned event all return to the same context control record. */
    for(unsigned mode=0;mode<4;++mode){
        xui_context raw=NULL;xui_widget widget=NULL;XL6_ForeignView* child=NULL;
        XL6_ForeignView* root=create(&raw,&widget,&child);bind(widget,root,child,READ);
        XL6_ForeignViewCallV1 call={0};xui_call_scope_t native={0};
        assert(xl6ForeignViewCallBeginV1(root,&call,&native,sizeof(native)));
        xui_widget canvas=NULL;xui_workflow workflow=NULL;xui_flow_graph graph=NULL;
        if(mode&1){
            xui_widget owner=NULL;assert(xuiWorkflowWidgetCreate(raw,&owner,NULL)==XUI_OK);
            workflow=xuiWorkflowWidgetGetWorkflow(owner);graph=xuiWorkflowGetGraph(workflow);
            /* The workflow's canvas and this extra canvas BOTH borrow graph. */
            xui_flow_graph_desc_t borrowed={0};borrowed.iSize=sizeof(borrowed);borrowed.pGraph=graph;
            assert(xuiFlowGraphWidgetCreate(raw,&canvas,&borrowed)==XUI_OK);
        }else{
            assert(xuiFlowGraphWidgetCreate(raw,&canvas,NULL)==XUI_OK);graph=xuiFlowGraphWidgetGetGraph(canvas);
        }
        assert(graph);xui_flow_node_desc_t node={0};node.iSize=sizeof(node);node.sId="cycle";
        assert(xuiFlowGraphAddNode(graph,&node,NULL)==XUI_OK);
        xvalue* boxed=xl6ForeignViewBoxV1(root,778);assert(boxed);
        assert(xuiFlowGraphSetNodeConfig(graph,"cycle",boxed)==XUI_OK);
        assert(xuiFlowGraphCommandSetNodeConfig(graph,"cycle",boxed)==XUI_OK);
        if(mode>=2){
            assert(xuiFlowGraphSelectNode(graph,"cycle",1)==XUI_OK && xuiFlowGraphDeleteSelection(graph)==XUI_OK);
            assert(xuiFlowGraphUndo(graph)==XUI_OK); /* old config now survives in redo group */
        }
        if(workflow){
            xvalue* schema=xrtValueObject();xvalue* copy=xrtValueClone(boxed);assert(schema && copy);
            assert(xrtValueObjectSetTake(schema,(xstrview){"owner",5},&copy) && !copy);
            xui_workflow_node_type_desc_t type={0};type.iSize=sizeof(type);type.sId="native-type";type.pConfigSchema=schema;
            assert(xuiWorkflowRegisterNodeType(workflow,&type,NULL)==XUI_OK);xrtValueRelease(schema);
            xui_workflow_variable_desc_t variable={0};variable.iSize=sizeof(variable);variable.sId="owner";variable.pDefaultValue=boxed;
            assert(xuiWorkflowAddVariable(workflow,&variable,NULL)==XUI_OK);
        }
        xrtValueRelease(boxed);assert(xl6ForeignViewCallEndV1(&call));
        unsigned edges=0;xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(xuiContextTraceOwnership(raw,count_edge,&edges));
        /* No duplicated graph slots through workflow's borrowed canvases. */
        assert(edges==(mode==0?4u:mode==1?6u:mode==2?5u:7u));
        assert(xrtOwnershipScopeEnd(&freeze));
        assert(xl6ForeignViewReleaseV1(child) && xl6ForeignViewReleaseV1(root));
        const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),xl6OwnershipDomainForeignViewValueV1Get(),
            xl6OwnershipDomainNativeCallbackV1Get(),xl6OwnershipDomainClosureV1Get()};
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,4);assert(catalog);XL6_OwnershipPlan* plan=NULL;
        assert(xl6OwnershipPlanTryCreateV6(xl6ForeignViewOwnershipV1(root),xl6OwnershipCatalogProbeV1,discover,true,catalog,&plan)>0 && plan);
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
        XL6_OwnershipStepResult step=XL6_OWNERSHIP_STEP_BUSY;
        for(unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i)step=xl6OwnershipPlanFinalizeV2(plan);
        assert(step==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanTryDestroy(plan)>0);
        xl6OwnershipCatalogDestroyV1(catalog);assert(xl6ForeignViewDrainV1());
    }
}
int main(void)
{
    size_t index=0;
#define XGE_HOST_XRT_SLOT(name) symbols[index++].address=(xge_host_xrt_proc)(name);
#include <xge_host_xrt_slots.inc>
#undef XGE_HOST_XRT_SLOT
    assert(index==sizeof(symbols)/sizeof(symbols[0]));
    const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
    assert(xgeHostXrtBind(&contract,resolve,NULL,NULL)==XGE_HOST_XRT_OK);
    assert(xgeHostXrtRuntimeIdentity()==(xge_host_xrt_proc)xrtVersion);
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));api=xl6NativeCallbackRuntimeV1Get();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);calls();cycle();payload_cycles();xrtClearError();xrtMemDebugSnapshot(&after);
    assert(!xl6ForeignViewPendingV1(0) && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("hosted XUI and resident userdata: typed events, graph/workflow payload cycles, borrowed canvas exclusion, native close and zero steady allocations passed");return 0;
}
