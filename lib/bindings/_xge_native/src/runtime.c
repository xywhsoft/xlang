/* Shared runtime binding and resource policies only. Drawing/UI methods are
 * declared directly against native symbols, without per-method adapters. */
#include <xge3_runtime.h>
#include <xge_host_xrt_contract.h>
#include <__xl_runtime/xl6_host_xrt.h>
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>

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
static bool destroy_context(void* raw){xuiDestroy(raw);return true;}
static bool destroy_widget(void* raw){xuiWidgetDestroy(raw);return true;}
static bool context_begin(void* raw,void* storage){return xuiCallScopeBegin(raw,storage)==XUI_OK;}
static bool context_end(void* storage){return xuiCallScopeEnd(storage)==XUI_OK;}
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
    if(!ready || ready->Adapter!=exact)return false;
    *adapter=exact;*preparation=ready;return true;
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
