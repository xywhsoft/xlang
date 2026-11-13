/* Shared runtime binding and resource policies only. Drawing/UI methods are
 * declared directly against native symbols, without per-method adapters. */
#include <xge3_runtime.h>
#if defined(XL6_XGE_STATIC)
#include <xge_static_xrt_contract.h>
#else
#include <xge_host_xrt_contract.h>
#include <__xl_runtime/xl6_host_xrt.h>
#endif
#include <__xl_runtime/xl_foreign_error.h>
#include <__xl_runtime/xl6_callback_failure.h>
static void report_resource_failure(int status,const char* operation);

#if !defined(XL6_XGE_STATIC)
static xge_host_xrt_proc resolve(void* unused,const char* name)
{
    (void)unused;return (xge_host_xrt_proc)xl6HostXrtResolveV1(name);
}
#endif
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
#if defined(XL6_XGE_STATIC)
    static const xge_host_xrt_contract_t contract=XGE_STATIC_XRT_LOCAL_CONTRACT_INIT;
    const xge_host_xrt_contract_t* expected=&xgeStaticXrtContractV1;
    if(expected->size!=contract.size||expected->revision!=contract.revision||
        expected->dataModel!=contract.dataModel||expected->coreLayout!=contract.coreLayout||
        !expected->headerSha256||strcmp(expected->headerSha256,contract.headerSha256))
        return bind_failure(XGE_HOST_XRT_INVALID_CONTRACT,"static-xrt-abi","Static XGE and program XRT ABI do not match");
    return 0;
#else
    static const xge_host_xrt_contract_t contract=XGE_HOST_XRT_LOCAL_CONTRACT_INIT;
    const char* missing=NULL;int result=xgeHostXrtBind(&contract,resolve,NULL,&missing);
    if(result==XGE_HOST_XRT_OK)return 0;
    const char* reason=result==XGE_HOST_XRT_MISSING_SYMBOL ? "XGE requires an unavailable host XRT symbol" :
        result==XGE_HOST_XRT_DIFFERENT_RUNTIME ? "XGE is bound to a different XRT runtime" :
        result==XGE_HOST_XRT_BUSY ? "XGE runtime binding is already active" : "XGE and the host XRT ABI do not match";
    return bind_failure(result,missing?missing:"bind",reason);
#endif
}
static bool watch_context(void* raw,void* storage,void(*invalidate)(void*),void* user)
{
    int status=xuiContextLifetimeWatch(raw,storage,invalidate,user);
    if(status!=XUI_OK){report_resource_failure(status,"xuiContextLifetimeWatch");return false;}
    /* Language roots require the atomic owned creation contract. A legacy C
     * Context cannot be admitted. Physical observation remains attached on
     * qualification failure so the already-transferred owner can retire. */
    if(!xuiGetProxyOwned(raw)){
        report_resource_failure(XUI_ERROR_INVALID_STATE,"xui.Context.owned");
    }
    return true;
}
static bool watch_widget(void* raw,void* storage,void(*invalidate)(void*),void* user)
{return xuiWidgetLifetimeWatch(raw,storage,invalidate,user)==XUI_OK;}
static void unwatch(void* storage){xuiLifetimeUnwatch(storage);}
static bool destroy_context(void* raw)
{bool consumed=false;int status=xuiDestroyOwnerChecked(raw,&consumed);if(status!=XUI_STATUS_DEFERRED)report_resource_failure(status,"xuiDestroyOwnerChecked");return consumed;}
static bool request_context(void* raw)
{int status=xuiRequestClose(raw);if(status!=XUI_OK)report_resource_failure(status,"xuiRequestClose");return status==XUI_OK;}
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
        sizeof(xui_call_scope_t),_Alignof(xui_call_scope_t),context_begin,context_end,context_trace,context_busy,
        XL6_FOREIGN_VIEW_RELIABLE_OWNED_WATCH|XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY|
        XL6_FOREIGN_VIEW_DESTROY_OWNS_NATIVE_SCOPE|XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|
        XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,request_context};
    return &value;
}
const XL6_ForeignViewDescriptorV1* xl6Xui3WidgetPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui_widget",
        sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_widget,unwatch,destroy_widget,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};
    return &value;
}
/* Registry-owned children share their Context's real ownership directory.
 * The adapter is lifetime bookkeeping, not a business-method forwarding layer.
 * Rollback removes the registry owner and separately reports callback errors. */
#define XL6_XUI_ICON_POLICY(Name,Prefix) \
static bool watch_##Name(void* raw,void* storage,void(*invalidate)(void*),void* user) \
{int status=Prefix##LifetimeWatch(raw,storage,invalidate,user);if(status!=XUI_OK)report_resource_failure(status,#Prefix "LifetimeWatch");return status==XUI_OK;} \
static bool destroy_##Name(void* raw) \
{bool accepted=false;int status=Prefix##RemoveOwnerChecked(raw,&accepted);if(status!=XUI_OK)report_resource_failure(status,#Prefix "RemoveOwnerChecked");return accepted;} \
const XL6_ForeignViewDescriptorV1* xl6Xui3##Name##PolicyGet(void) \
{static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui." #Name, \
sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_##Name,unwatch,destroy_##Name, \
0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};return &value;}
XL6_XUI_ICON_POLICY(IconCategory,xuiIconCategory)
XL6_XUI_ICON_POLICY(Icon,xuiIcon)
#undef XL6_XUI_ICON_POLICY
void xl6Xui3RollbackIconCategory(xui_icon_category raw)
{(void)destroy_IconCategory(raw);}
void xl6Xui3RollbackIcon(xui_icon raw)
{(void)destroy_Icon(raw);}

/* Context owns each manager exactly once. These are distinct composite child
 * views, not Widget casts and not independent native RC roots. Context's real
 * directory traces their callback/font/layout edges; tracing them again from
 * a child descriptor would invent a second owning edge. Calls borrow the
 * actual parent Context root scope and the native business body holds its
 * ManagerScope through the complete callback tail. */
#define XL6_XUI_MANAGER_POLICY(Name,Prefix) \
static bool watch_##Name(void* raw,void* storage,void(*invalidate)(void*),void* user) \
{int status=Prefix##LifetimeWatch(raw,storage,invalidate,user);if(status!=XUI_OK)report_resource_failure(status,#Prefix "LifetimeWatch");return status==XUI_OK;} \
static bool destroy_##Name(void* raw) \
{bool accepted=false;int status=Prefix##DestroyOwnerChecked(raw,&accepted);if(status!=XUI_OK)report_resource_failure(status,#Prefix "DestroyOwnerChecked");return accepted;} \
const XL6_ForeignViewDescriptorV1* xl6Xui3##Name##PolicyGet(void) \
{static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui." #Name, \
sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_##Name,unwatch,destroy_##Name, \
0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};return &value;}
XL6_XUI_MANAGER_POLICY(MsgBox,xuiMsgBox)
XL6_XUI_MANAGER_POLICY(Toast,xuiToast)
XL6_XUI_MANAGER_POLICY(FileDialog,xuiFileDialog)
XL6_XUI_MANAGER_POLICY(MsgTip,xuiMsgTip)
#undef XL6_XUI_MANAGER_POLICY

/* These composite children live in the Context's actual native directory.
 * Their descriptor only watches identity; Context traces each native edge
 * once, and the existing parent operation scope pins complete native tails. */
#define XL6_XUI_DOCUMENT_CHILD_POLICY(Name,Prefix) \
static bool watch_##Name(void* raw,void* storage,void(*invalidate)(void*),void* user) \
{int status=Prefix##LifetimeWatch(raw,storage,invalidate,user);if(status!=XUI_OK)report_resource_failure(status,#Prefix "LifetimeWatch");return status==XUI_OK;} \
static bool destroy_##Name(void* raw) \
{bool consumed=false;int status=Prefix##ReleaseOwnerChecked(raw,&consumed);if(status!=XUI_OK && status!=XUI_STATUS_DEFERRED)report_resource_failure(status,#Prefix "ReleaseOwnerChecked");return consumed;} \
const XL6_ForeignViewDescriptorV1* xl6Xui3##Name##PolicyGet(void) \
{static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui." #Name, \
sizeof(xui_lifetime_link_t),_Alignof(xui_lifetime_link_t),watch_##Name,unwatch,destroy_##Name, \
0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};return &value;}
#if XUI_ENABLE_DOCUMENT_VIEW
XL6_XUI_DOCUMENT_CHILD_POLICY(DocumentImageRequest,xuiDocumentImageResource)
static bool destroy_document_renderer(void* raw)
{bool consumed=false;int status=xuiDocumentRendererReleaseOwnerChecked(raw,&consumed);if(status!=XUI_STATUS_DEFERRED)report_resource_failure(status,"xuiDocumentRendererReleaseOwnerChecked");return consumed;}
static bool request_document_renderer(void* raw)
{int status=xuiDocumentRendererRequestClose(raw);report_resource_failure(status,"xuiDocumentRendererRequestClose");return status==XUI_OK;}
static bool trace_document_renderer(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return xuiDocumentRendererTraceOwnership((xui_document_renderer)raw,visit,user);}
static bool busy_document_renderer(const void* raw)
{return xuiDocumentRendererOwnershipBusy((xui_document_renderer)raw);}
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentRendererPolicyGet(void)
{
    /* Unique ownership transferred to the language, not Context ownership or
     * native RC. Context is weak and may expire; native operations then reject
     * admission. Resident active calls defer root destruction; native bodies
     * pin their own calls, so no synthetic NativeCallBegin/Watch is invented. */
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.documentRenderer",
        0,1,NULL,NULL,destroy_document_renderer,0,1,NULL,NULL,trace_document_renderer,busy_document_renderer,
        XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,request_document_renderer};
    return &value;
}
#else
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentImageRequestPolicyGet(void){return NULL;}
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentRendererPolicyGet(void){return NULL;}
#endif
#if XUI_ENABLE_WEBVIEW
XL6_XUI_DOCUMENT_CHILD_POLICY(DocumentHtmlInteraction,xuiDocumentHtmlInteraction)
XL6_XUI_DOCUMENT_CHILD_POLICY(DocumentWebProvider,xuiDocumentWebProvider)
#else
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentHtmlInteractionPolicyGet(void){return NULL;}
const XL6_ForeignViewDescriptorV1* xl6Xui3DocumentWebProviderPolicyGet(void){return NULL;}
#endif
#undef XL6_XUI_DOCUMENT_CHILD_POLICY

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
        "xui.table_cell_output",0,1,NULL,NULL,destroy_cell,0,1,NULL,NULL,cell_trace,cell_busy,0,NULL,NULL};
    return &value;
}
/* Exact cold private package addition proposed for Root to merge.
 * Native15 API per family is now implemented in the standalone components.
 * No Watch / extra Native scope: one ForeignView owns one actual shared RC,
 * resident activity defers view close; Native methods pin their own callouts. */
#define XN_DATA_OBJECT_POLICY(Name,Prefix,Deferred,Family) \
static bool destroy_##Name(void* raw) \
{bool consumed=false;int status=Prefix##ReleaseOwnerChecked(raw,&consumed); \
 if(status!=Deferred){report_resource_failure(status,#Prefix "ReleaseOwnerChecked");}return consumed;} \
static bool trace_##Name(const void* raw,XL6_ModuleOwnershipVisit visit,void* user) \
{return visit(Prefix##Ownership((void*)raw),user);} \
static bool busy_##Name(const void* raw) \
{xrtownershipref ref=Prefix##Ownership((void*)raw); \
 const xrtownershipquiescencev1* q=Prefix##OwnershipQuiescenceV1(ref);return !q || q->Busy(ref.Data);} \
const XL6_ForeignViewDescriptorV1* Name##PolicyGet(void) \
{static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,Family, \
 0,1,NULL,NULL,destroy_##Name,0,1,NULL,NULL,trace_##Name,busy_##Name, \
 XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,NULL};return &value;}
XN_DATA_OBJECT_POLICY(xl6Xge3DataObject,xgeDataObject,XGE_STATUS_DEFERRED,"xge.dataObject")
#if XGE_ENABLE_XUI
XN_DATA_OBJECT_POLICY(xl6Xui3DataObject,xuiDataObject,XUI_STATUS_DEFERRED,"xui.dataObject")
#else
const XL6_ForeignViewDescriptorV1* xl6Xui3DataObjectPolicyGet(void){return NULL;}
#endif
#undef XN_DATA_OBJECT_POLICY

/* One adapter lookup authenticates which exact family/type owns this ref.
 * Both family getters admit their private generation nodes as well as objects.
 * They do not accept raw origins or arbitrary Ops merely by shape. */
static const xrtownershipadapterv1* transfer_exact(xrtownershipref ref,bool* ui)
{
    *ui=false;const xrtownershipadapterv1* exact=xgeDataObjectOwnershipAdapterV1(ref);
#if XGE_ENABLE_XUI
    if(!exact){exact=xuiDataObjectOwnershipAdapterV1(ref);*ui=exact!=NULL;}
#endif
    return exact;
}
static bool transfer_domain_resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** finish)
{
    (void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;bool ui=false;
    const xrtownershipadapterv1* exact=transfer_exact(ref,&ui);if(!exact)return false;
    const xrtownershippreparationv1* ready=xgeDataObjectOwnershipPreparationV1(ref);
    const xrtownershipfinishv2* complete=xgeDataObjectOwnershipCompletionV2(ref);
#if XGE_ENABLE_XUI
    if(ui){ready=xuiDataObjectOwnershipPreparationV1(ref);complete=xuiDataObjectOwnershipCompletionV2(ref);}
#endif
    if(!ready || ready->size!=sizeof(*ready) || ready->Adapter!=exact || !complete ||
       complete->V1.size!=sizeof(*complete) || complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG ||
       complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady)return false;
    *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;
}
static bool transfer_domain_read(xrtownershipref ref)
{
    bool result=xgeDataObjectOwnershipReadReady(ref);
#if XGE_ENABLE_XUI
    result=result || xuiDataObjectOwnershipReadReady(ref);
#endif
    return result;
}
static bool transfer_domain_finalization(xrtownershipref ref,const xrtownershipadapterv1* adapter,
    const xrtownershipfinalizationv1** output)
{
    *output=NULL;bool ui=false;const xrtownershipadapterv1* exact=transfer_exact(ref,&ui);
    if(!exact)return true;
    if(exact!=adapter)return false;
    const xrtownershipfinalizationv1* value=xgeDataObjectOwnershipFinalizationV1(ref);
#if XGE_ENABLE_XUI
    if(ui)value=xuiDataObjectOwnershipFinalizationV1(ref);
#endif
    if(!value || value->size!=sizeof(*value) || value->AbiTag!=XRT_OWNERSHIP_FINALIZATION_V1_TAG ||
       value->Adapter!=adapter || !value->CanStep || !value->Step)return false;
    *output=value;return true;
}
const XL6_OwnershipDomainV1* xl6Xge3TransferDomainV1Get(void)
{
    static const XL6_OwnershipDomainV2 value={{sizeof(value),NULL,transfer_domain_resolve,transfer_domain_read},
        XL6_OWNERSHIP_DOMAIN_V2_TAG,transfer_domain_finalization};return &value.V1;
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
        proxy_domain_resolve(ref,policies,adapter,preparation,finish) ||
        transfer_domain_resolve(ref,policies,adapter,preparation,finish) ||
        xl6Xui3DocumentModelDomainV1Get()->Resolve(ref,policies,adapter,preparation,finish);
}
static bool xui_domain_read(xrtownershipref ref)
{return cell_domain_read(ref) || proxy_domain_read(ref) || transfer_domain_read(ref) || xl6Xui3DocumentModelDomainV1Get()->AdmitsRead(ref);}
static bool document_domain_finalization(xrtownershipref,const xrtownershipadapterv1*,const xrtownershipfinalizationv1**);
static bool xui_domain_finalization(xrtownershipref ref,const xrtownershipadapterv1* adapter,
    const xrtownershipfinalizationv1** output)
{
    bool ui=false;
    if(transfer_exact(ref,&ui))return transfer_domain_finalization(ref,adapter,output);
    return document_domain_finalization(ref,adapter,output);
}

const XL6_OwnershipDomainV1* xl6Xui3OwnershipDomainV1Get(void)
{
    /* Context roots can reach resource nodes without a separate language
     * resource view. Admission therefore travels with the complete UI root. */
    static const XL6_OwnershipDomainV2 value={{sizeof(value),NULL,xui_domain_resolve,xui_domain_read},
        XL6_OWNERSHIP_DOMAIN_V2_TAG,xui_domain_finalization};
    return &value.V1;
}
static void report_resource_failure(int status,const char* operation)
{
    if(status==XUI_OK)return;
    xl6ForeignStatusFailureV1(status,operation);
    xl6_error_result failure={false,xrtTakeError()};
    if(!xl6ErrorScopeTryPropagateFailureV1(&failure))xl6CallbackFailureRecordV1(failure);
}
#if XUI_ENABLE_DOCUMENT
/* These are native RC roots, not independently watchable Widgets. Each
 * ForeignView owns one real native reference. Its resident active pin defers
 * close across callbacks; native business methods provide their own admission
 * and activity protocol. Do not add a second retain/scope around each call.
 * Trace exposes the independent native node once, including its exact graph. */
#define XUI_DOCUMENT_MODEL_POLICY(Name,Prefix,Family) \
static bool destroy_##Name(void* raw) \
{bool consumed=false;int status=Prefix##ReleaseOwnerChecked(raw,&consumed); \
 if(status!=XUI_STATUS_DEFERRED){report_resource_failure(status,#Prefix "ReleaseOwnerChecked");}return consumed;} \
static bool trace_##Name(const void* raw,XL6_ModuleOwnershipVisit visit,void* user) \
{return visit(Prefix##Ownership((void*)raw),user);} \
static bool busy_##Name(const void* raw) \
{xrtownershipref ref=Prefix##Ownership((void*)raw); \
 const xrtownershipquiescencev1* q=xuiDocumentModelOwnershipQuiescenceV1(ref); \
 return !q || q->Busy(ref.Data);} \
const XL6_ForeignViewDescriptorV1* xl6Xui3##Name##PolicyGet(void) \
{static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,Family, \
 0,1,NULL,NULL,destroy_##Name,0,1,NULL,NULL,trace_##Name,busy_##Name, \
 XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE,NULL,NULL};return &value;}
XUI_DOCUMENT_MODEL_POLICY(Document,xuiDocument,"xui.document")
XUI_DOCUMENT_MODEL_POLICY(DocumentSnapshot,xuiDocumentSnapshot,"xui.documentSnapshot")
XUI_DOCUMENT_MODEL_POLICY(DocumentChangeSet,xuiDocumentChangeSet,"xui.documentChangeSet")
XUI_DOCUMENT_MODEL_POLICY(DocumentPrepare,xuiDocumentPrepare,"xui.documentPrepare")
XUI_DOCUMENT_MODEL_POLICY(DocumentFragment,xuiDocumentFragment,"xui.documentFragment")
XUI_DOCUMENT_MODEL_POLICY(DocumentSourceIndex,xuiDocumentSourceIndex,"xui.documentSourceIndex")
XUI_DOCUMENT_MODEL_POLICY(DocumentTxn,xuiDocumentTxn,"xui.documentTxn")
#undef XUI_DOCUMENT_MODEL_POLICY
static bool document_domain_resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** finish)
{
    (void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;
    const xrtownershipadapterv1* exact=xuiDocumentModelOwnershipAdapterV1(ref);
    if(!exact)return false;
    const xrtownershippreparationv1* ready=xuiDocumentModelOwnershipPreparationV1(ref);
    const xrtownershipfinishv2* complete=xuiDocumentModelOwnershipCompletionV2(ref);
    if(!ready || ready->size!=sizeof(*ready) || ready->Adapter!=exact ||
       !complete || complete->V1.size!=sizeof(*complete) || complete->AbiTag!=XRT_OWNERSHIP_FINISH_V2_TAG ||
       complete->V1.Adapter!=exact || !complete->DropChecked || !complete->DropReady)return false;
    *adapter=exact;*preparation=ready;*finish=&complete->V1;return true;
}
static bool document_domain_read(xrtownershipref ref)
{return xuiDocumentModelOwnershipReadReady(ref);}
static bool document_domain_finalization(xrtownershipref ref,const xrtownershipadapterv1* adapter,
    const xrtownershipfinalizationv1** output)
{
    *output=NULL;
    const xrtownershipfinalizationv1* value=xuiDocumentModelOwnershipFinalizationV1(ref);
    if(!value)return true;
    if(value->size!=sizeof(*value) || value->AbiTag!=XRT_OWNERSHIP_FINALIZATION_V1_TAG ||
       value->Adapter!=adapter || !value->CanStep || !value->Step)return false;
    *output=value;return true;
}
#else
#define XUI_DOCUMENT_MODEL_DISABLED(Name) \
const XL6_ForeignViewDescriptorV1* xl6Xui3##Name##PolicyGet(void){return NULL;}
XUI_DOCUMENT_MODEL_DISABLED(Document)
XUI_DOCUMENT_MODEL_DISABLED(DocumentSnapshot)
XUI_DOCUMENT_MODEL_DISABLED(DocumentChangeSet)
XUI_DOCUMENT_MODEL_DISABLED(DocumentPrepare)
XUI_DOCUMENT_MODEL_DISABLED(DocumentFragment)
XUI_DOCUMENT_MODEL_DISABLED(DocumentSourceIndex)
XUI_DOCUMENT_MODEL_DISABLED(DocumentTxn)
#undef XUI_DOCUMENT_MODEL_DISABLED
static bool document_domain_resolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** finish)
{(void)ref;(void)policies;*adapter=NULL;*preparation=NULL;*finish=NULL;return false;}
static bool document_domain_read(xrtownershipref ref){(void)ref;return false;}
static bool document_domain_finalization(xrtownershipref ref,const xrtownershipadapterv1* adapter,
    const xrtownershipfinalizationv1** output)
{(void)ref;(void)adapter;*output=NULL;return true;}
#endif
const XL6_OwnershipDomainV1* xl6Xui3DocumentModelDomainV1Get(void)
{
    static const XL6_OwnershipDomainV2 value={{sizeof(value),NULL,document_domain_resolve,document_domain_read},
        XL6_OWNERSHIP_DOMAIN_V2_TAG,document_domain_finalization};
    return &value.V1;
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
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,Family,0,1,NULL,NULL,Destroy,0,1,NULL,NULL,Trace,NULL,0,NULL,NULL};return &value;}
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
static bool draw_busy(const void* raw)
{
    xrtownershipref ref=xuiDrawOwnership((xui_draw_context)raw);
    const xrtownershipquiescencev1* quiet=xuiProxyResourceOwnershipQuiescenceV1(ref);
    return !quiet || quiet->Busy(ref.Data);
}
const XL6_ForeignViewDescriptorV1* xl6Xui3DrawPolicyGet(void)
{
    /* Scope pins are independent from snapshot refs: the last call tail must
     * finish a requested End even while an ownership plan retains storage. */
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.draw",
        0,1,NULL,NULL,destroy_draw,sizeof(xui_draw_scope_t),_Alignof(xui_draw_scope_t),draw_begin,draw_end,draw_trace,draw_busy,XL6_FOREIGN_VIEW_DEFER_ACTIVE_DESTROY,NULL,NULL};
    return &value;
}
static bool destroy_text_shape(void* raw)
{bool consumed=false;int status=xuiTextShapeReleaseOwnerChecked(raw,&consumed);if(status!=XUI_STATUS_DEFERRED)report_resource_failure(status,"xuiTextShapeReleaseOwnerChecked");return consumed;}
static bool text_shape_trace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{return visit(xuiTextShapeOwnership(raw),user);}
static bool text_shape_busy(const void* raw)
{
    xrtownershipref ref=xuiTextShapeOwnership(raw);
    const xrtownershipquiescencev1* quiet=xuiProxyResourceOwnershipQuiescenceV1(ref);
    return !quiet || quiet->Busy(ref.Data);
}
const XL6_ForeignViewDescriptorV1* xl6Xui3TextShapePolicyGet(void)
{
    /* The immutable public record is the real payload address, not a wrapper.
     * Its native RC owner protects it throughout resident input call scopes. */
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.textShape",
        0,1,NULL,NULL,destroy_text_shape,0,1,NULL,NULL,text_shape_trace,text_shape_busy,
        XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE,NULL,NULL};
    return &value;
}
static bool destroy_path(void* raw){xuiPathDestroy(raw);return true;}
/* Scope members are native borrowed editors. XGE removes destroyed editors
 * and detaches all remaining members when the scope closes. No language
 * callback is installed here, so this independent root has no owning edges. */
static bool destroy_find_scope(void* raw){xuiCodeFindScopeDestroy(raw);return true;}
const XL6_ForeignViewDescriptorV1* xl6Xui3CodeFindScopePolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.codeFindScope",
        0,1,NULL,NULL,destroy_find_scope,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};return &value;
}
const XL6_ForeignViewDescriptorV1* xl6Xui3PathPolicyGet(void)
{
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,"xui.path",
        0,1,NULL,NULL,destroy_path,0,1,NULL,NULL,NULL,NULL,0,NULL,NULL};return &value;
}
static bool destroy_font(void* raw){xgeFontFree(raw);return true;}
static bool destroy_texture(void* raw){xgeTextureFree(raw);return true;}
/* Before AdoptStorage succeeds Native does not own this xrt-allocated
 * record. Do not consume our reference while another C reference still
 * needs its address: retirement retries after those references are gone. */
static bool destroy_unadopted_font(void* raw)
{
    xge_font font=raw;
    bool owned=xgeFontIsOwned(font);
    if(!owned && font->iRefCount>1){xrtSetErrorKind(XERR_STATE);return false;}
    xgeFontFree(font);if(!owned)xrtFree(font);return true;
}
static bool destroy_unadopted_texture(void* raw)
{
    xge_texture texture=raw;
    bool owned=xgeTextureIsOwned(texture);
    if(!owned && texture->iRefCount>1){xrtSetErrorKind(XERR_STATE);return false;}
    xgeTextureFree(texture);if(!owned)xrtFree(texture);return true;
}
static bool destroy_image(void* raw){xgeImageFree(raw);xrtFree(raw);return true;}
static bool destroy_material(void* raw){xgeMaterialFree(raw);xrtFree(raw);return true;}
#define RESOURCE_POLICY(Name,Family,Destroy,DestroyUnadopted) \
const XL6_ForeignViewDescriptorV1* Name(void) { \
    static const XL6_ForeignViewDescriptorV1 value={sizeof(value),XL6_FOREIGN_VIEW_V1_TAG,Family,0,1,NULL,NULL,Destroy,0,1,NULL,NULL,NULL,NULL,0,DestroyUnadopted,NULL};return &value;}
RESOURCE_POLICY(xl6Xge3FontPolicyGet,"xge.font",destroy_font,destroy_unadopted_font)
RESOURCE_POLICY(xl6Xge3TexturePolicyGet,"xge.texture",destroy_texture,destroy_unadopted_texture)
RESOURCE_POLICY(xl6Xge3ImagePolicyGet,"xge.image",destroy_image,destroy_image)
RESOURCE_POLICY(xl6Xge3MaterialPolicyGet,"xge.material",destroy_material,destroy_material)
#if XGE_ENABLE_SVG
static bool destroy_svg(void* raw){xgeSvgDestroy(raw);return true;}
static bool destroy_svg_cache(void* raw){xgeSvgCacheDestroy(raw);return true;}
RESOURCE_POLICY(xl6Xge3SvgPolicyGet,"xge.svg",destroy_svg,NULL)
RESOURCE_POLICY(xl6Xge3SvgCachePolicyGet,"xge.svg_cache",destroy_svg_cache,NULL)
#else
const XL6_ForeignViewDescriptorV1* xl6Xge3SvgPolicyGet(void){return NULL;}
const XL6_ForeignViewDescriptorV1* xl6Xge3SvgCachePolicyGet(void){return NULL;}
#endif
#if XGE_ENABLE_SHAPE_EX
static bool destroy_shape(void* raw){xgeShapeExDestroy(raw);return true;}
static bool destroy_scene(void* raw){xgeShapeExSceneDestroy(raw);return true;}
static bool destroy_measure(void* raw){xgeShapeExPathMeasureDestroy(raw);return true;}
RESOURCE_POLICY(xl6Xge3ShapePolicyGet,"xge.shape_ex",destroy_shape,NULL)
RESOURCE_POLICY(xl6Xge3ShapeScenePolicyGet,"xge.shape_ex_scene",destroy_scene,NULL)
RESOURCE_POLICY(xl6Xge3PathMeasurePolicyGet,"xge.shape_ex_path_measure",destroy_measure,NULL)
#else
const XL6_ForeignViewDescriptorV1* xl6Xge3ShapePolicyGet(void){return NULL;}
const XL6_ForeignViewDescriptorV1* xl6Xge3ShapeScenePolicyGet(void){return NULL;}
const XL6_ForeignViewDescriptorV1* xl6Xge3PathMeasurePolicyGet(void){return NULL;}
#endif
#if XGE_ENABLE_AUDIO
static bool destroy_audio_clip(void* raw){xgeAudioClipFree(raw);return true;}
RESOURCE_POLICY(xl6Xge3AudioClipPolicyGet,"xge.audio_clip",destroy_audio_clip,NULL)
#else
const XL6_ForeignViewDescriptorV1* xl6Xge3AudioClipPolicyGet(void){return NULL;}
#endif
#undef RESOURCE_POLICY
