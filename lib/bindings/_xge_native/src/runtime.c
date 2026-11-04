/* Shared runtime binding and resource policies only. Drawing/UI methods are
 * declared directly against native symbols, without per-method adapters. */
#include <xge3_runtime.h>
#include <xge_host_xrt_contract.h>
#include <__xl_runtime/xl6_host_xrt.h>
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>
static void report_resource_failure(int status,const char* operation);

static xge_host_xrt_proc resolve(void* unused,const char* name)
{
    (void)unused;return (xge_host_xrt_proc)xl6HostXrtResolveV1(name);
}
static int bind_failure(int result,const char* operation,const char* reason)
{
    xerrordesc description={0};description.Kind=XERR_STATE;description.Code=result;
    description.Domain="xlang.xge3";description.Operation=operation;description.Message=reason;
    xrtSetErrorTake(xrtErrorBuild(&description));return result;
}
int xl6Xge3RuntimeBindV1(void)
{
    if(!XGE_BUILD_ABI_FINGERPRINT || xgeCheckBuildCompatibility(XGE_BUILD_ABI_FINGERPRINT,XGE_FEATURES)!=XGE_OK)
        return bind_failure(XGE_ERROR_UNSUPPORTED,"native-abi","XGE and the declarative SDK ABI do not match");
    static const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
    const char* missing=NULL;int result=xgeHostXrtBind(&contract,resolve,NULL,&missing);
    if(result==XGE_HOST_XRT_OK)return 0;
    const char* reason=result==XGE_HOST_XRT_MISSING_SYMBOL ? "XGE requires an unavailable host XRT symbol" :
        result==XGE_HOST_XRT_DIFFERENT_RUNTIME ? "XGE is bound to a different XRT runtime" :
        result==XGE_HOST_XRT_BUSY ? "XGE runtime binding is already active" : "XGE and the host XRT ABI do not match";
    return bind_failure(result,missing?missing:"bind",reason);
}
static bool watch_context(void* raw,void* storage,void(*invalidate)(void*),void* user)
{return xuiContextLifetimeWatch(raw,storage,invalidate,user)==XUI_OK;}
static bool watch_widget(void* raw,void* storage,void(*invalidate)(void*),void* user)
{return xuiWidgetLifetimeWatch(raw,storage,invalidate,user)==XUI_OK;}
static void unwatch(void* storage){xuiLifetimeUnwatch(storage);}
static bool destroy_context(void* raw)
{bool consumed=false;int status=xuiDestroyOwnerChecked(raw,&consumed);report_resource_failure(status,"xuiDestroyOwnerChecked");return consumed;}
static bool destroy_widget(void* raw)
{int status=xuiWidgetDestroyChecked(raw);report_resource_failure(status,"xuiWidgetDestroyChecked");return status==XUI_OK;}
static bool context_begin(void* raw,void* storage)
{int status=xuiCallScopeBegin(raw,storage);if(status!=XUI_OK)report_resource_failure(status,"xuiCallScopeBegin");return status==XUI_OK;}
static bool context_end(void* storage)
{int status=xuiCallScopeEnd(storage);if(status!=XUI_OK)report_resource_failure(status,"xuiCallScopeEnd");return status==XUI_OK;}
static bool context_busy(const void* raw){return xuiContextOwnershipBusy((xui_context)raw);}
static bool context_trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return xuiContextTraceOwnership((xui_context)raw,visit,user);}
const XL6_ForeignViewDescriptorV1* xl6Xui3ContextPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui_context",
        sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_context,unwatch,destroy_context,
        sizeof(xui_call_scope_t),_Alignof(xui_call_scope_t),context_begin,context_end,context_trace,context_busy};
    return &value;
}
const XL6_ForeignViewDescriptorV1* xl6Xui3WidgetPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui_widget",
        sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_widget,unwatch,destroy_widget,0,1,NULL,NULL,NULL,NULL};
    return &value;
}
static bool destroy_cell(void* raw)
{
    bool consumed=false;
    int status=xuiTableViewCellOutputReleaseOwnerChecked(raw,&consumed);
    if(status!=XUI_OK){
        /* The native owner can be consumed even if its payload finalizer
         * fails. Report that checked failure separately from the consumption
         * result, including {false,NULL} after diagnostic allocation failure. */
        xl6ForeignStatusFailureV1(status,"xuiTableViewCellOutputReleaseOwnerChecked");
        xl6_error_result failure={false,xrtTakeError()};
        if(!xl6ErrorScopeTryPropagateFailureV1(&failure))xl6CallbackFailureRecordV1(failure);
    }
    return consumed;
}
static bool cell_trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xuiTableViewCellOutputOwnership(raw),user);}
static bool cell_busy(const void* raw){return xuiTableViewCellOutputBusy(raw);}
const XL6_ForeignViewDescriptorV1* xl6Xui3TableCellOutputPolicyGet(void)
{
    /* One real native owning reference protects this shared RC object.
     * External Release cannot invalidate it; heap Dispose is forbidden.
     * Its native RC node remains independent, with ALL of its real edges. */
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,
        "xui.table_cell_output",0,1,NULL,NULL,destroy_cell,0,1,NULL,NULL,cell_trace,cell_busy};
    return &value;
}
static bool cell_domain_resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** finish)
{
    (void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;
    const xrtownershipadapterv1* exact=xuiTableViewCellOutputOwnershipAdapterV1(ref);
    if(!exact)return false;
    const xrtownershippreparationv1* ready=xuiTableViewCellOutputOwnershipPreparationV1(ref);
    const xrtownershipfinishv2* complete=xuiTableViewCellOutputOwnershipCompletionV2(ref);
    if(!ready || ready->Adapter!=exact || !complete || complete->V1.size!=sizeof(*complete) ||
       complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG || complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady)return false;
    *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;
}
static bool cell_domain_read(xrtownershipref ref)
{
    const xrtownershipquiescencev1* exact=xuiTableViewCellOutputOwnershipQuiescenceV1(ref);
    return exact && exact->Ops==ref.Ops && !exact->Busy(ref.Data);
}
const XL6_OwnershipDomainV1* xuiTableViewCellOutputOwnershipDomainV1Get(void)
{
    /* Zero cold Load side effects. Resolve also accepts NULL policies when
     * reached through a live ForeignView root's pinned immutable domain. */
    static const XL6_OwnershipDomainV1 value={sizeof(value),NULL,cell_domain_resolve,cell_domain_read};
    return &value;
}
static bool proxy_domain_resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** finish)
{
    (void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;
    const xrtownershipadapterv1* exact=xuiProxyResourceOwnershipAdapterV1(ref);
    if(!exact)return false;
    const xrtownershippreparationv1* ready=xuiProxyResourceOwnershipPreparationV1(ref);
    const xrtownershipfinishv2* complete=xuiProxyResourceOwnershipCompletionV2(ref);
    if(!ready || ready->Adapter!=exact || !complete || complete->V1.size!=sizeof(*complete) ||
       complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG || complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady)return false;
    *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;
}
static bool proxy_domain_read(xrtownershipref ref)
{
    const xrtownershipquiescencev1* exact=xuiProxyResourceOwnershipQuiescenceV1(ref);
    return exact && exact->Ops==ref.Ops && !exact->Busy(ref.Data);
}
static bool xui_domain_resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** finish)
{
    return cell_domain_resolve(ref,policies,adapter,preparation,finish) ||
        proxy_domain_resolve(ref,policies,adapter,preparation,finish);
}
static bool xui_domain_read(xrtownershipref ref)
{return cell_domain_read(ref) || proxy_domain_read(ref);}
const XL6_OwnershipDomainV1* xl6Xui3OwnershipDomainV1Get(void)
{
    /* Context roots can reach resource nodes without a separate language
     * resource view. Admission therefore travels with the complete UI root. */
    static const XL6_OwnershipDomainV1 value={sizeof(value),NULL,xui_domain_resolve,xui_domain_read};
    return &value;
}
static void report_resource_failure(int status,const char* operation)
{
    if(status==XUI_OK)return;
    xl6ForeignStatusFailureV1(status,operation);
    xl6_error_result failure={false,xrtTakeError()};
    if(!xl6ErrorScopeTryPropagateFailureV1(&failure))xl6CallbackFailureRecordV1(failure);
}
static bool destroy_proxy(void* raw)
{bool consumed=false;int status=xuiProxyReleaseOwnerChecked(raw,&consumed);report_resource_failure(status,"xuiProxyReleaseOwnerChecked");return consumed;}
static bool destroy_xui_font(void* raw)
{bool consumed=false;int status=xuiFontReleaseOwnerChecked(raw,&consumed);report_resource_failure(status,"xuiFontReleaseOwnerChecked");return consumed;}
static bool destroy_surface(void* raw)
{bool consumed=false;int status=xuiSurfaceReleaseOwnerChecked(raw,&consumed);report_resource_failure(status,"xuiSurfaceReleaseOwnerChecked");return consumed;}
static bool proxy_trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xuiProxyOwnership((xui_proxy)raw),user);}
static bool xui_font_trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xuiFontOwnership((xui_font)raw),user);}
static bool surface_trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xuiSurfaceOwnership((xui_surface)raw),user);}
#define XUI_SHARED_POLICY(Name,Family,Destroy,Trace) \
const XL6_ForeignViewDescriptorV1* Name(void) { \
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,Family,0,1,NULL,NULL,Destroy,0,1,NULL,NULL,Trace,NULL};return &value;}
/* An owning RC reference protects each shared record. A language close
 * consumes that owner; native active pins and other owners keep storage alive.
 * Native quiescence/preparation belongs to the independent native graph node. */
XUI_SHARED_POLICY(xl6Xui3ProxyPolicyGet,"xui.proxy",destroy_proxy,proxy_trace)
XUI_SHARED_POLICY(xl6Xui3FontPolicyGet,"xui.font",destroy_xui_font,xui_font_trace)
XUI_SHARED_POLICY(xl6Xui3SurfacePolicyGet,"xui.surface",destroy_surface,surface_trace)
#undef XUI_SHARED_POLICY
static bool destroy_draw(void* raw)
{bool consumed=false;int status=xuiDrawReleaseOwnerChecked(raw,&consumed);report_resource_failure(status,"xuiDrawReleaseOwnerChecked");return consumed;}
static bool draw_trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xuiDrawOwnership((xui_draw_context)raw),user);}
static bool draw_begin(void* raw,void* storage)
{int status=xuiDrawCallScopeBegin(raw,storage);if(status!=XUI_OK)report_resource_failure(status,"xuiDrawCallScopeBegin");return status==XUI_OK;}
static bool draw_end(void* storage)
{int status=xuiDrawCallScopeEnd(storage);if(status!=XUI_OK)report_resource_failure(status,"xuiDrawCallScopeEnd");return status==XUI_OK;}
const XL6_ForeignViewDescriptorV1* xl6Xui3DrawPolicyGet(void)
{
    /* Scope pins are independent from snapshot refs: the last call tail must
     * finish a requested End even while an ownership plan retains storage. */
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.draw",
        0,1,NULL,NULL,destroy_draw,sizeof(xui_draw_scope_t),_Alignof(xui_draw_scope_t),draw_begin,draw_end,draw_trace,NULL};
    return &value;
}
static bool destroy_path(void* raw){xuiPathDestroy(raw);return true;}
const XL6_ForeignViewDescriptorV1* xl6Xui3PathPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.path",
        0,1,NULL,NULL,destroy_path,0,1,NULL,NULL,NULL,NULL};return &value;
}
static bool destroy_font(void* raw){xgeFontFree(raw);return true;}
static bool destroy_texture(void* raw){xgeTextureFree(raw);return true;}
static bool destroy_image(void* raw){xgeImageFree(raw);xrtFree(raw);return true;}
static bool destroy_material(void* raw){xgeMaterialFree(raw);xrtFree(raw);return true;}
#define RESOURCE_POLICY(Name,Family,Destroy) \
const XL6_ForeignViewDescriptorV1* Name(void) { \
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,Family,0,1,NULL,NULL,Destroy,0,1,NULL,NULL,NULL,NULL};return &value;}
RESOURCE_POLICY(xl6Xge3FontPolicyGet,"xge.font",destroy_font)
RESOURCE_POLICY(xl6Xge3TexturePolicyGet,"xge.texture",destroy_texture)
RESOURCE_POLICY(xl6Xge3ImagePolicyGet,"xge.image",destroy_image)
RESOURCE_POLICY(xl6Xge3MaterialPolicyGet,"xge.material",destroy_material)
#if XGE_ENABLE_SHAPE_EX
static bool destroy_shape(void* raw){xgeShapeExDestroy(raw);return true;}
static bool destroy_scene(void* raw){xgeShapeExSceneDestroy(raw);return true;}
static bool destroy_measure(void* raw){xgeShapeExPathMeasureDestroy(raw);return true;}
RESOURCE_POLICY(xl6Xge3ShapePolicyGet,"xge.shape_ex",destroy_shape)
RESOURCE_POLICY(xl6Xge3ShapeScenePolicyGet,"xge.shape_ex_scene",destroy_scene)
RESOURCE_POLICY(xl6Xge3PathMeasurePolicyGet,"xge.shape_ex_path_measure",destroy_measure)
#else
const XL6_ForeignViewDescriptorV1* xl6Xge3ShapePolicyGet(void){return NULL;}
const XL6_ForeignViewDescriptorV1* xl6Xge3ShapeScenePolicyGet(void){return NULL;}
const XL6_ForeignViewDescriptorV1* xl6Xge3PathMeasurePolicyGet(void){return NULL;}
#endif
#if XGE_ENABLE_AUDIO
static bool destroy_audio_clip(void* raw){xgeAudioClipFree(raw);return true;}
RESOURCE_POLICY(xl6Xge3AudioClipPolicyGet,"xge.audio_clip",destroy_audio_clip)
#else
const XL6_ForeignViewDescriptorV1* xl6Xge3AudioClipPolicyGet(void){return NULL;}
#endif
#undef RESOURCE_POLICY
