#define XRT_MODULE_ONCE
#include "src/common/xl6_xrt.h"
#include "xl6_native_com.h"
#include "xl6_foreign_view.h"
#include "xl6_callback_failure.h"
#include "src/api/xl_foreign_error.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>

static int transferError(xerrkind kind)
{ xrtSetErrorKind(kind); return -1; }

static bool transferServicesValid(const xn_transfer_services_v1* services)
{
    if(!services || services->iSize!=sizeof(*services) ||
       services->iAbiTag!=XN_TRANSFER_SERVICES_V1_TAG ||
       (services->iFlags&~XN_TRANSFER_PROCESS_RESIDENT))return false;
    if((services->iFlags&XN_TRANSFER_PROCESS_RESIDENT) ? services->pCodeCapability!=NULL :
       services->pCodeCapability==NULL)return false;
    const XL6_OwnershipDomainV1* domain=services->pOwnershipDomain;
    return !domain || (xl6OwnershipDomainValidV1(domain) && !domain->Load && domain->Resolve && domain->AdmitsRead);
}

#if defined(_WIN32)
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <oleidl.h>

/* These fixed iface identities are data in the resident component. No
 * QueryInterface call is forwarded into the publisher or exposes its vtable. */
static const IID transferUnknown={0,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const IID transferData={0x10e,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const IID transferSource={0x121,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const IID transferTarget={0x122,0,0,{0xc0,0,0,0,0,0,0,0x46}};

typedef struct TransferCommon {
    xatomic64 references;
    XL6_ForeignView* view;
    void* native;
    xn_transfer_native_owner_v1 owner;
} TransferCommon;
typedef struct TransferData { IDataObject iface; TransferCommon common; } TransferData;
typedef struct TransferSource { IDropSource iface; TransferCommon common; } TransferSource;
typedef struct TransferTarget { IDropTarget iface; TransferCommon common; } TransferTarget;
_Static_assert(offsetof(TransferData,common)==offsetof(TransferSource,common),"COM allocation prefix");
_Static_assert(offsetof(TransferData,common)==offsetof(TransferTarget,common),"COM allocation prefix");
typedef struct TransferCall {
    TransferCommon* common;
    XL6_ForeignViewCallV1 call;
    xl6_error_scope errors;
} TransferCall;

static void transferPublishFailure(xl6_error_result failure)
{
    if(failure.ok)return;
    if(!xl6ErrorScopeTryPropagateFailureV1(&failure))xl6CallbackFailureRecordV1(failure);
}
static ULONG transferAddRef(TransferCommon* common)
{
    uint64_t value=xrtAtomic64Load(&common->references,XMEMORY_RELAXED);
    while(value){
        /* COM's ULONG result is diagnostic only: it cannot reject a valid
         * AddRef. Preserve the real 64-bit reference and saturate the result.
         * True 64-bit overflow is a host contract violation, like CodeLease. */
        if(value==UINT64_MAX)abort();
        if(xrtAtomic64CompareExchange(&common->references,&value,value+1,XMEMORY_RELAXED,XMEMORY_RELAXED))
            return value+1>ULONG_MAX ? ULONG_MAX:(ULONG)(value+1);
    }
    return 0;
}
static ULONG transferRelease(TransferCommon* common)
{
    uint64_t previous=xrtAtomic64FetchSub(&common->references,1,XMEMORY_ACQ_REL);
    if(!previous)abort();
    uint64_t remaining=previous-1;
    if(!remaining){
        /* No access to common after this call: native owner consumption may
         * synchronously free its containing allocation on the creator. */
        XL6_ForeignView* view=common->view;
        if(!xl6ForeignViewReleaseV1(view))
            transferPublishFailure((xl6_error_result){false,xrtTakeError()});
    }
    return remaining>ULONG_MAX ? ULONG_MAX:(ULONG)remaining;
}
static HRESULT transferQuery(TransferCommon* common,void* iface,REFIID requested,
    const IID* exact,void** output)
{
    if(!output)return E_POINTER;
    *output=NULL;
    if(!requested || (!IsEqualIID(requested,&transferUnknown) && !IsEqualIID(requested,exact)))return E_NOINTERFACE;
    if(!transferAddRef(common))return E_UNEXPECTED;
    *output=iface;return S_OK;
}
static bool transferBusy(const void* raw)
{
    const TransferCommon* common=raw;
    return common->owner.iCreatorThread!=xrtThreadCurrentId() || !common->owner.onReady(common->native);
}
static bool transferTrace(const void* raw,XL6_ModuleOwnershipVisit visit,void* user)
{
    const TransferCommon* common=raw;
    return common->owner.onTrace(common->native,visit,user);
}
static bool transferDestroy(void* raw)
{
    TransferCommon* common=raw;
    if(transferBusy(common))return false;
    bool consumed=false;
    int status=common->owner.onReleaseOwnerChecked(common->native,&consumed);
    if(status<0 || (status==0 && !consumed) || (status>0 && (status!=1 || consumed))){
        /* A valid negative status is a checked failure even without a payload.
         * Do not replace that NULL first outcome with an invented diagnostic. */
        if(status>=0 && !xrtGetError())xrtSetErrorKind(XERR_STATE);
        transferPublishFailure((xl6_error_result){false,xrtTakeError()});
    }
    if(consumed){
        /* common is the second member; preserve a concrete allocation pointer
         * without a guessed iface offset or an incompatible function cast. */
        void* allocation=(unsigned char*)common-offsetof(TransferData,common);
        xrtFree(allocation);
    }
    return consumed;
}
static const XL6_ForeignViewDescriptorV1 transferDescriptor={
    .size=sizeof(XL6_ForeignViewDescriptorV1),.abiTag=XL6_FOREIGN_VIEW_V1_TAG,
    .family="resident COM transfer capsule",.watchAlignment=1,.Destroy=transferDestroy,
    .callAlignment=1,.TraceRaw=transferTrace,.Busy=transferBusy,
    .flags=XL6_FOREIGN_VIEW_DESTROY_REPORTS_FAILURE|XL6_FOREIGN_VIEW_CONTINUABLE_FINALIZE
};
static HRESULT transferBegin(TransferCommon* common,TransferCall* call)
{
    if(common->owner.iCreatorThread!=xrtThreadCurrentId())return RPC_E_WRONG_THREAD;
    if(!transferAddRef(common))return E_UNEXPECTED;
    if(!xl6ErrorScopeBegin(&call->errors)){
        transferPublishFailure((xl6_error_result){false,xrtTakeError()});
        transferRelease(common);return E_FAIL;
    }
    call->common=common;
    if(!xl6ForeignViewCallBeginV1(common->view,&call->call,NULL,0)){
        xl6ErrorScopeCaptureFailure(&call->errors);
        transferRelease(common);
        transferPublishFailure(xl6ErrorScopeEnd(&call->errors));
        call->common=NULL;return E_FAIL;
    }
    return S_OK;
}
static HRESULT transferEnd(TransferCall* call,HRESULT nativeStatus)
{
    /* Capture before cleanup, including the independent failed callback bit.
     * HRESULT business failures without XRT diagnostics remain ordinary COM
     * results; an escaping language/cleanup failure uses the checked channel. */
    if(xrtGetError())xl6ErrorScopeCaptureFailure(&call->errors);
    if(!xl6ForeignViewCallEndV1(&call->call))xl6ErrorScopeCaptureFailure(&call->errors);
    transferRelease(call->common);call->common=NULL;
    xl6_error_result result=xl6ErrorScopeEnd(&call->errors);
    bool ok=result.ok;transferPublishFailure(result);
    return !ok && SUCCEEDED(nativeStatus) ? E_FAIL:nativeStatus;
}

#define TRANSFER_IDENTITY(Prefix,Type,Interface,Iid) \
static HRESULT STDMETHODCALLTYPE Prefix##Query(Interface* self,REFIID iid,void** out) \
{ Type* value=CONTAINING_RECORD(self,Type,iface);return transferQuery(&value->common,self,iid,&Iid,out); } \
static ULONG STDMETHODCALLTYPE Prefix##AddRef(Interface* self) \
{ return transferAddRef(&CONTAINING_RECORD(self,Type,iface)->common); } \
static ULONG STDMETHODCALLTYPE Prefix##Release(Interface* self) \
{ return transferRelease(&CONTAINING_RECORD(self,Type,iface)->common); }
TRANSFER_IDENTITY(data,TransferData,IDataObject,transferData)
TRANSFER_IDENTITY(source,TransferSource,IDropSource,transferSource)
TRANSFER_IDENTITY(target,TransferTarget,IDropTarget,transferTarget)
#undef TRANSFER_IDENTITY

#define TRANSFER_BEGIN(Type,Interface) \
    TransferCall call={0};TransferCommon* common=&CONTAINING_RECORD(self,Type,iface)->common; \
    HRESULT entered=transferBegin(common,&call);if(FAILED(entered))return entered; \
    Interface* native=common->native

static HRESULT STDMETHODCALLTYPE dataGetData(IDataObject* self,FORMATETC* format,STGMEDIUM* medium)
{
    TRANSFER_BEGIN(TransferData,IDataObject);
    HRESULT status=IDataObject_GetData(native,format,medium),result=transferEnd(&call,status);
    if(SUCCEEDED(status) && FAILED(result) && medium){ReleaseStgMedium(medium);memset(medium,0,sizeof(*medium));}
    return result;
}
static HRESULT STDMETHODCALLTYPE dataGetDataHere(IDataObject* self,FORMATETC* format,STGMEDIUM* medium)
{ TRANSFER_BEGIN(TransferData,IDataObject);return transferEnd(&call,IDataObject_GetDataHere(native,format,medium)); }
static HRESULT STDMETHODCALLTYPE dataQueryGetData(IDataObject* self,FORMATETC* format)
{ TRANSFER_BEGIN(TransferData,IDataObject);return transferEnd(&call,IDataObject_QueryGetData(native,format)); }
static HRESULT STDMETHODCALLTYPE dataCanonical(IDataObject* self,FORMATETC* input,FORMATETC* output)
{ TRANSFER_BEGIN(TransferData,IDataObject);return transferEnd(&call,IDataObject_GetCanonicalFormatEtc(native,input,output)); }
static HRESULT STDMETHODCALLTYPE dataSetData(IDataObject* self,FORMATETC* format,STGMEDIUM* medium,BOOL release)
{ TRANSFER_BEGIN(TransferData,IDataObject);return transferEnd(&call,IDataObject_SetData(native,format,medium,release)); }
static HRESULT STDMETHODCALLTYPE dataEnumFormats(IDataObject* self,DWORD direction,IEnumFORMATETC** output)
{
    TRANSFER_BEGIN(TransferData,IDataObject);
    HRESULT status=IDataObject_EnumFormatEtc(native,direction,output),result=transferEnd(&call,status);
    if(SUCCEEDED(status) && FAILED(result) && output && *output){IEnumFORMATETC_Release(*output);*output=NULL;}
    return result;
}
static HRESULT STDMETHODCALLTYPE dataAdvise(IDataObject* self,FORMATETC* format,DWORD flags,IAdviseSink* sink,DWORD* connection)
{ TRANSFER_BEGIN(TransferData,IDataObject);return transferEnd(&call,IDataObject_DAdvise(native,format,flags,sink,connection)); }
static HRESULT STDMETHODCALLTYPE dataUnadvise(IDataObject* self,DWORD connection)
{ TRANSFER_BEGIN(TransferData,IDataObject);return transferEnd(&call,IDataObject_DUnadvise(native,connection)); }
static HRESULT STDMETHODCALLTYPE dataEnumAdvise(IDataObject* self,IEnumSTATDATA** output)
{
    TRANSFER_BEGIN(TransferData,IDataObject);
    HRESULT status=IDataObject_EnumDAdvise(native,output),result=transferEnd(&call,status);
    if(SUCCEEDED(status) && FAILED(result) && output && *output){IEnumSTATDATA_Release(*output);*output=NULL;}
    return result;
}
static const IDataObjectVtbl dataVtable={dataQuery,dataAddRef,dataRelease,dataGetData,dataGetDataHere,
    dataQueryGetData,dataCanonical,dataSetData,dataEnumFormats,dataAdvise,dataUnadvise,dataEnumAdvise};
static HRESULT STDMETHODCALLTYPE sourceContinue(IDropSource* self,BOOL escape,DWORD keys)
{ TRANSFER_BEGIN(TransferSource,IDropSource);return transferEnd(&call,IDropSource_QueryContinueDrag(native,escape,keys)); }
static HRESULT STDMETHODCALLTYPE sourceFeedback(IDropSource* self,DWORD effect)
{ TRANSFER_BEGIN(TransferSource,IDropSource);return transferEnd(&call,IDropSource_GiveFeedback(native,effect)); }
static const IDropSourceVtbl sourceVtable={sourceQuery,sourceAddRef,sourceRelease,sourceContinue,sourceFeedback};
static HRESULT STDMETHODCALLTYPE targetEnter(IDropTarget* self,IDataObject* data,DWORD keys,POINTL point,DWORD* effect)
{ TRANSFER_BEGIN(TransferTarget,IDropTarget);return transferEnd(&call,IDropTarget_DragEnter(native,data,keys,point,effect)); }
static HRESULT STDMETHODCALLTYPE targetOver(IDropTarget* self,DWORD keys,POINTL point,DWORD* effect)
{ TRANSFER_BEGIN(TransferTarget,IDropTarget);return transferEnd(&call,IDropTarget_DragOver(native,keys,point,effect)); }
static HRESULT STDMETHODCALLTYPE targetLeave(IDropTarget* self)
{ TRANSFER_BEGIN(TransferTarget,IDropTarget);return transferEnd(&call,IDropTarget_DragLeave(native)); }
static HRESULT STDMETHODCALLTYPE targetDrop(IDropTarget* self,IDataObject* data,DWORD keys,POINTL point,DWORD* effect)
{ TRANSFER_BEGIN(TransferTarget,IDropTarget);return transferEnd(&call,IDropTarget_Drop(native,data,keys,point,effect)); }
static const IDropTargetVtbl targetVtable={targetQuery,targetAddRef,targetRelease,targetEnter,targetOver,targetLeave,targetDrop};
#undef TRANSFER_BEGIN

static bool transferOwnerValid(const xn_transfer_native_owner_v1* owner)
{
    return owner && owner->iSize==sizeof(*owner) && owner->iAbiTag==XN_TRANSFER_NATIVE_OWNER_V1_TAG &&
        owner->iCreatorThread==xrtThreadCurrentId() && owner->onReady && owner->onTrace && owner->onReleaseOwnerChecked;
}
/* Each factory below sets its concrete typed vtable, with no erased dispatcher
 * or table/function-pointer conversion. Allocation and FV admission are cold. */
#define TRANSFER_FACTORY(Name,Type,Vtable) \
static int Name(const xn_transfer_services_v1* services,void* native,const xn_transfer_native_owner_v1* owner,void** output) \
{ \
    if(!output)return transferError(XERR_ARGUMENT); \
    *output=NULL; \
    if(!transferServicesValid(services) || !native || !transferOwnerValid(owner))return transferError(XERR_ARGUMENT); \
    Type* value=xrtCalloc(1,sizeof(*value));if(!value)return -1; \
    value->iface.lpVtbl=&Vtable;xrtAtomic64Init(&value->common.references,1);value->common.native=native;value->common.owner=*owner; \
    value->common.view=xl6ForeignViewCreateRootFromLeaseV1(&value->common,&transferDescriptor, \
        services->pOwnershipDomain,(XL6_CodeLease*)services->pCodeCapability); \
    if(!value->common.view){xrtFree(value);return -1;} \
    *output=&value->iface;return 0; \
}
TRANSFER_FACTORY(transferWrapData,TransferData,dataVtable)
TRANSFER_FACTORY(transferWrapSource,TransferSource,sourceVtable)
TRANSFER_FACTORY(transferWrapTarget,TransferTarget,targetVtable)
#undef TRANSFER_FACTORY
#if defined(XL6_NATIVE_COM_TEST)
/* Isolated test-only metadata access, absent from the production component.
 * Simulates the 32-bit boundary without billions of live COM references. */
void xl6NativeComTestReferencesSet(void* iface,uint64_t count)
{
    TransferCommon* common=(void*)((unsigned char*)iface+offsetof(TransferData,common));
    xrtAtomic64Store(&common->references,count,XMEMORY_RELAXED);
}
uint64_t xl6NativeComTestReferencesGet(const void* iface)
{
    const TransferCommon* common=(const void*)((const unsigned char*)iface+offsetof(TransferData,common));
    return xrtAtomic64Load(&common->references,XMEMORY_RELAXED);
}
#endif
#else
static int transferWrapUnsupported(const xn_transfer_services_v1* services,void* native,
    const xn_transfer_native_owner_v1* owner,void** output)
{
    (void)services;(void)native;(void)owner;
    if(!output)return transferError(XERR_ARGUMENT);
    *output=NULL;return transferError(XERR_UNSUPPORTED);
}
#define transferWrapData transferWrapUnsupported
#define transferWrapSource transferWrapUnsupported
#define transferWrapTarget transferWrapUnsupported
#endif

static bool transferServices(XL6_CodeLease* lease,const XL6_OwnershipDomainV1* domain,
    uint32_t flags,xn_transfer_services_v1* output)
{
    if(!output){transferError(XERR_ARGUMENT);return false;}
    memset(output,0,sizeof(*output));
    xn_transfer_services_v1 value={sizeof(value),XN_TRANSFER_SERVICES_V1_TAG,flags,lease,domain,
        transferWrapData,transferWrapSource,transferWrapTarget};
    if(!transferServicesValid(&value)){transferError(XERR_ARGUMENT);return false;}
    *output=value;return true;
}
bool xl6TransferServicesFromLeaseV1(XL6_CodeLease* lease,const XL6_OwnershipDomainV1* domain,
    xn_transfer_services_v1* output)
{return transferServices(lease,domain,0,output);}
bool xl6TransferServicesProcessResidentV1(const XL6_OwnershipDomainV1* domain,xn_transfer_services_v1* output)
{return transferServices(NULL,domain,XN_TRANSFER_PROCESS_RESIDENT,output);}
