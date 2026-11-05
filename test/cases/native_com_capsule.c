#define XRT_MODULE_ONCE
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_native_com.h"
#include "src/runtime/xl6_foreign_view.h"
#include "src/runtime/xl6_callback_failure.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <oleidl.h>
extern void xl6NativeComTestReferencesSet(void*,uint64_t);
extern uint64_t xl6NativeComTestReferencesGet(const void*);

typedef struct State { unsigned calls,queries,releases,traces,tails;bool ready,reenter,failCallback;int releaseStatus;void* resident;xvalue* capture; } State;
typedef struct Fake { union { IDataObject data;IDropSource source;IDropTarget target; } iface;State* state; } Fake;
static const IID iidData={0x10e,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const IID iidSource={0x121,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const IID iidTarget={0x122,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static State* state(void* value){return ((Fake*)value)->state;}
static bool ready(const void* value){return ((const Fake*)value)->state->ready;}
static bool trace(const void* value,xn_transfer_ownership_visitor visitor,void* user)
{const State* s=((const Fake*)value)->state;return !s->capture || visitor(xrtValueOwnership(s->capture),user);}
static int releaseOwner(void* value,bool* consumed)
{State* s=state(value);*consumed=false;if(!s->ready)return 1;++s->releases;*consumed=true;xvalue* capture=s->capture;s->capture=NULL;xrtValueRelease(capture);return s->releaseStatus;}
static xn_transfer_native_owner_v1 owner(void)
{xn_transfer_native_owner_v1 v={sizeof(v),XN_TRANSFER_NATIVE_OWNER_V1_TAG,xrtThreadCurrentId(),ready,trace,releaseOwner};return v;}
static void hit(void* value,unsigned bit){state(value)->calls|=1u<<bit;}
#define IDENTITY(Prefix,Type) \
static HRESULT STDMETHODCALLTYPE Prefix##Query(Type* self,REFIID iid,void** out){(void)iid;if(out)*out=NULL;++state(self)->queries;return E_NOINTERFACE;} \
static ULONG STDMETHODCALLTYPE Prefix##Add(Type* self){(void)self;assert(!"native AddRef must not replace the unique duty");return 0;} \
static ULONG STDMETHODCALLTYPE Prefix##Drop(Type* self){(void)self;assert(!"native Release must use checked owner protocol");return 0;}
IDENTITY(d,IDataObject)
IDENTITY(s,IDropSource)
IDENTITY(t,IDropTarget)
#undef IDENTITY
static HRESULT STDMETHODCALLTYPE get(IDataObject* self,FORMATETC* f,STGMEDIUM* m){assert(f&&m);hit(self,0);memset(m,0,sizeof(*m));return DV_E_FORMATETC;}
static HRESULT STDMETHODCALLTYPE here(IDataObject* self,FORMATETC* f,STGMEDIUM* m){assert(f&&m);hit(self,1);return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE query(IDataObject* self,FORMATETC* f){assert(f);hit(self,2);State* p=state(self);if(p->reenter){assert(IDataObject_Release((IDataObject*)p->resident)==1);assert(!p->releases);++p->tails;}if(p->failCallback){xl6_error_result failure={false,NULL};assert(xl6ErrorScopeTryPropagateFailureV1(&failure));}return S_OK;}
static HRESULT STDMETHODCALLTYPE canon(IDataObject* self,FORMATETC* i,FORMATETC* o){assert(i&&o);hit(self,3);o->ptd=NULL;return DATA_S_SAMEFORMATETC;}
static HRESULT STDMETHODCALLTYPE set(IDataObject* self,FORMATETC* f,STGMEDIUM* m,BOOL r){assert(f&&m&&!r);hit(self,4);return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE formats(IDataObject* self,DWORD d,IEnumFORMATETC** o){assert(d==DATADIR_GET&&o);hit(self,5);*o=NULL;return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE advise(IDataObject* self,FORMATETC* f,DWORD flags,IAdviseSink* sink,DWORD* conn){assert(f&&flags==3&&!sink&&conn);hit(self,6);*conn=0;return OLE_E_ADVISENOTSUPPORTED;}
static HRESULT STDMETHODCALLTYPE unadvise(IDataObject* self,DWORD conn){assert(conn==9);hit(self,7);return OLE_E_ADVISENOTSUPPORTED;}
static HRESULT STDMETHODCALLTYPE advices(IDataObject* self,IEnumSTATDATA** o){assert(o);hit(self,8);*o=NULL;return OLE_E_ADVISENOTSUPPORTED;}
static const IDataObjectVtbl dataVtable={dQuery,dAdd,dDrop,get,here,query,canon,set,formats,advise,unadvise,advices};
static HRESULT STDMETHODCALLTYPE cont(IDropSource* self,BOOL esc,DWORD keys){assert(esc&&keys==7);hit(self,0);return DRAGDROP_S_CANCEL;}
static HRESULT STDMETHODCALLTYPE feedback(IDropSource* self,DWORD effect){assert(effect==DROPEFFECT_COPY);hit(self,1);return DRAGDROP_S_USEDEFAULTCURSORS;}
static const IDropSourceVtbl sourceVtable={sQuery,sAdd,sDrop,cont,feedback};
static HRESULT STDMETHODCALLTYPE enter(IDropTarget* self,IDataObject* data,DWORD keys,POINTL point,DWORD* effect){assert(!data&&keys==3&&point.x==4&&point.y==5&&effect);hit(self,0);*effect=DROPEFFECT_COPY;return S_OK;}
static HRESULT STDMETHODCALLTYPE over(IDropTarget* self,DWORD keys,POINTL point,DWORD* effect){assert(keys==3&&point.x==4&&point.y==5&&effect);hit(self,1);return S_OK;}
static HRESULT STDMETHODCALLTYPE leave(IDropTarget* self){hit(self,2);return S_OK;}
static HRESULT STDMETHODCALLTYPE drop(IDropTarget* self,IDataObject* data,DWORD keys,POINTL point,DWORD* effect){assert(!data&&keys==3&&point.x==4&&point.y==5&&effect);hit(self,3);return S_OK;}
static const IDropTargetVtbl targetVtable={tQuery,tAdd,tDrop,enter,over,leave,drop};
static void methods(const xn_transfer_services_v1* services)
{
    assert(IsEqualIID(&iidData,&IID_IDataObject)&&IsEqualIID(&iidSource,&IID_IDropSource)&&IsEqualIID(&iidTarget,&IID_IDropTarget));
    xn_transfer_native_owner_v1 own=owner();State ds={.ready=true};Fake df={.iface.data={&dataVtable},.state=&ds};void* output=NULL;
    assert(!services->wrapDataObject(services,&df,&own,&output));IDataObject* data=output;
    xl6NativeComTestReferencesSet(data,UINT32_MAX-1);
    assert(IDataObject_AddRef(data)==ULONG_MAX&&xl6NativeComTestReferencesGet(data)==UINT32_MAX);
    assert(IDataObject_AddRef(data)==ULONG_MAX&&xl6NativeComTestReferencesGet(data)==UINT64_C(0x100000000));
    assert(IDataObject_Release(data)==ULONG_MAX&&xl6NativeComTestReferencesGet(data)==UINT32_MAX);
    assert(IDataObject_Release(data)==ULONG_MAX-1&&xl6NativeComTestReferencesGet(data)==UINT32_MAX-1);
    xl6NativeComTestReferencesSet(data,1);assert(!ds.releases);
    void* identity=NULL;assert(IDataObject_QueryInterface(data,&iidData,&identity)==S_OK&&identity==data);assert(IDataObject_Release((IDataObject*)identity)==1);
    assert(IDataObject_QueryInterface(data,&iidSource,&identity)==E_NOINTERFACE&&!identity&&!ds.queries);
    FORMATETC f={0},canonical={0};STGMEDIUM m={0};IEnumFORMATETC* enumerator=NULL;IEnumSTATDATA* advice=NULL;DWORD conn=99;
    assert(xrtMemDebugFailAfter(0));
    assert(IDataObject_GetData(data,&f,&m)==DV_E_FORMATETC);assert(IDataObject_GetDataHere(data,&f,&m)==E_NOTIMPL);
    assert(IDataObject_QueryGetData(data,&f)==S_OK);assert(IDataObject_GetCanonicalFormatEtc(data,&f,&canonical)==DATA_S_SAMEFORMATETC);
    assert(IDataObject_SetData(data,&f,&m,FALSE)==E_NOTIMPL);assert(IDataObject_EnumFormatEtc(data,DATADIR_GET,&enumerator)==E_NOTIMPL);
    assert(IDataObject_DAdvise(data,&f,3,NULL,&conn)==OLE_E_ADVISENOTSUPPORTED&&!conn);assert(IDataObject_DUnadvise(data,9)==OLE_E_ADVISENOTSUPPORTED);
    assert(IDataObject_EnumDAdvise(data,&advice)==OLE_E_ADVISENOTSUPPORTED);assert(ds.calls==511&&!xrtGetError());
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(!IDataObject_Release(data)&&ds.releases==1);
    State ss={.ready=true};Fake sf={.iface.source={&sourceVtable},.state=&ss};assert(!services->wrapDropSource(services,&sf,&own,&output));IDropSource* source=output;
    assert(IDropSource_QueryInterface(source,&iidSource,&identity)==S_OK&&identity==source);assert(IDropSource_Release((IDropSource*)identity)==1);
    assert(IDropSource_QueryContinueDrag(source,TRUE,7)==DRAGDROP_S_CANCEL);assert(IDropSource_GiveFeedback(source,DROPEFFECT_COPY)==DRAGDROP_S_USEDEFAULTCURSORS);
    assert(!IDropSource_Release(source)&&ss.calls==3&&ss.releases==1&&!ss.queries);
    State ts={.ready=true};Fake tf={.iface.target={&targetVtable},.state=&ts};assert(!services->wrapDropTarget(services,&tf,&own,&output));IDropTarget* target=output;POINTL point={4,5};DWORD effect=0;
    assert(IDropTarget_QueryInterface(target,&iidTarget,&identity)==S_OK&&identity==target);assert(IDropTarget_Release((IDropTarget*)identity)==1);
    assert(IDropTarget_DragEnter(target,NULL,3,point,&effect)==S_OK&&effect==DROPEFFECT_COPY);assert(IDropTarget_DragOver(target,3,point,&effect)==S_OK);
    assert(IDropTarget_DragLeave(target)==S_OK);assert(IDropTarget_Drop(target,NULL,3,point,&effect)==S_OK);
    assert(!IDropTarget_Release(target)&&ts.calls==15&&ts.releases==1&&!ts.queries);
}
static DWORD WINAPI worker(void* value)
{
    IDataObject* data=value;FORMATETC format={0};assert(IDataObject_QueryGetData(data,&format)==RPC_E_WRONG_THREAD);
    assert(IDataObject_AddRef(data)==2&&IDataObject_Release(data)==1);assert(!IDataObject_Release(data));return 0;
}
static DWORD WINAPI referenceWorker(void* value)
{IDataObject* data=value;for(unsigned i=0;i<10000;++i){assert(IDataObject_AddRef(data)>1);assert(IDataObject_Release(data)>0);}return 0;}
static void lifetime(const xn_transfer_services_v1* services)
{
    xn_transfer_native_owner_v1 own=owner();State s={0};Fake f={.iface.data={&dataVtable},.state=&s};void* out=NULL;
    assert(!services->wrapDataObject(services,&f,&own,&out));HANDLE thread=CreateThread(NULL,0,worker,out,0,NULL);assert(thread);
    assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);assert(!s.releases&&xl6ForeignViewPendingV1(0));
    assert(xl6ForeignViewDrainV1()&&!s.releases);s.ready=true;assert(xl6ForeignViewDrainV1()&&s.releases==1&&!xl6ForeignViewPendingV1(0));
    State reentrant={.ready=true,.reenter=true};Fake rf={.iface.data={&dataVtable},.state=&reentrant};assert(!services->wrapDataObject(services,&rf,&own,&out));
    reentrant.resident=out;FORMATETC format={0};assert(IDataObject_QueryGetData((IDataObject*)out,&format)==S_OK);
    assert(reentrant.tails==1&&reentrant.releases==1&&!xl6ForeignViewPendingV1(0));
    State concurrent={.ready=true};Fake cf={.iface.data={&dataVtable},.state=&concurrent};assert(!services->wrapDataObject(services,&cf,&own,&out));
    HANDLE workers[4];for(unsigned i=0;i<4;++i){workers[i]=CreateThread(NULL,0,referenceWorker,out,0,NULL);assert(workers[i]);}
    assert(WaitForMultipleObjects(4,workers,TRUE,20000)==WAIT_OBJECT_0);for(unsigned i=0;i<4;++i)CloseHandle(workers[i]);
    assert(xl6NativeComTestReferencesGet(out)==1&&!concurrent.releases);assert(!IDataObject_Release((IDataObject*)out)&&concurrent.releases==1);
}
typedef struct TraceProbe {xrtownershipref child;unsigned roots,edges;} TraceProbe;
static bool childTrace(xrtownershipref ref,void* user)
{TraceProbe* p=user;if(ref.Data==p->child.Data && ref.Ops==p->child.Ops)++p->edges;return true;}
static bool rootTrace(xrtownershipref ref,void* user)
{TraceProbe* p=user;++p->roots;return ref.Ops->Trace(ref.Data,childTrace,user);}
static void graphAndCosts(const xn_transfer_services_v1* services)
{
    xn_transfer_native_owner_v1 own=owner();State s={.ready=true};Fake f={.iface.data={&dataVtable},.state=&s};void* output=NULL;
    xmemdebugsnapshot before={0},after={0},finished={0};xrtMemDebugSnapshot(&before);
    assert(!services->wrapDataObject(services,&f,&own,&output));xrtMemDebugSnapshot(&after);
    printf("warm resident wrap: allocations=%llu payloadBytes=%zu\n",(unsigned long long)(after.AllocCount-before.AllocCount),after.LiveBytes-before.LiveBytes);
    assert(!IDataObject_Release((IDataObject*)output));xrtMemDebugSnapshot(&finished);assert(finished.LiveBytes==before.LiveBytes);
    s.capture=xrtValueString((xstrview){"actual retained slot",20});assert(s.capture);s.releases=0;
    TraceProbe probe={.child=xrtValueOwnership(s.capture)};assert(!services->wrapDataObject(services,&f,&own,&output));
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6ForeignViewDiscoverV1(NULL,rootTrace,&probe));assert(xrtOwnershipScopeEnd(&freeze));
    assert(probe.roots==1&&probe.edges==1);assert(!IDataObject_Release((IDataObject*)output)&&s.releases==1&&!s.capture);
}
static void failures(const xn_transfer_services_v1* services)
{
    xn_transfer_native_owner_v1 own=owner();void* out=NULL;
    State callback={.ready=true,.failCallback=true};Fake cb={.iface.data={&dataVtable},.state=&callback};
    assert(!services->wrapDataObject(services,&cb,&own,&out));FORMATETC format={0};
    xl6_error_scope method={0};assert(xl6ErrorScopeBegin(&method)&&xrtMemDebugFailAfter(0));
    assert(IDataObject_QueryGetData((IDataObject*)out,&format)==E_FAIL);xrtMemDebugFailClear();
    xl6_error_result methodResult=xl6ErrorScopeEnd(&method);assert(!methodResult.ok&&!methodResult.error&&!callback.releases);
    callback.failCallback=false;xerror* ambient=xrtErrorCreate(XERR_ARGUMENT,"test",17,"pre-existing ambient");assert(ambient);xrtSetErrorTake(ambient);
    assert(IDataObject_QueryGetData((IDataObject*)out,&format)==S_OK&&xrtGetError()==ambient);xrtClearError();
    assert(!IDataObject_Release((IDataObject*)out)&&callback.releases==1);
    State s={.ready=true,.releaseStatus=-16};Fake f={.iface.data={&dataVtable},.state=&s};assert(!services->wrapDataObject(services,&f,&own,&out));
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(!IDataObject_Release((IDataObject*)out));
    xl6_error_result error=xl6ErrorScopeEnd(&scope);assert(!error.ok&&!error.error&&s.releases==1);assert(!xl6ForeignViewPendingV1(0));
    s.releases=0;assert(!services->wrapDataObject(services,&f,&own,&out));assert(!IDataObject_Release((IDataObject*)out));
    uint64_t count=0;assert(xl6CallbackFailureTakeV1(&error,&count)&&!error.ok&&!error.error&&count==1&&s.releases==1);
    s.releaseStatus=0;
    for(unsigned failure=0;failure<6;++failure){
        s.releases=0;assert(xrtMemDebugFailAfter(failure));out=(void*)1;int status=services->wrapDataObject(services,&f,&own,&out);
        xrtMemDebugFailClear();if(status){assert(!out&&!s.releases);xrtClearError();}else assert(!IDataObject_Release((IDataObject*)out)&&s.releases==1);
    }
    xn_transfer_native_owner_v1 bad=own;bad.iAbiTag=0;out=(void*)1;assert(services->wrapDataObject(services,&f,&bad,&out)<0&&!out);xrtClearError();
    for(unsigned i=0;i<5;++i){
        xn_transfer_services_v1 invalid=*services;bad=own;
        if(i==0)--invalid.iSize;
        if(i==1)invalid.iAbiTag=0;
        if(i==2)invalid.iFlags|=2;
        if(i==3)++bad.iCreatorThread;
        if(i==4)bad.onTrace=NULL;
        unsigned releases=s.releases;out=(void*)1;
        assert(services->wrapDataObject(&invalid,&f,&bad,&out)<0&&!out&&s.releases==releases);xrtClearError();
    }
}
#endif
static unsigned portableAcquires,portableReleases;
static int portableAcquire(void* context){(void)context;++portableAcquires;return 1;}
static void portableRelease(void* context){(void)context;++portableReleases;}
int main(void)
{
    assert(xrtMemDebugEnable(true));xn_transfer_services_v1 services={0};
    memset(&services,0xa5,sizeof(services));assert(!xl6TransferServicesFromLeaseV1(NULL,NULL,&services));
    xn_transfer_services_v1 zero={0};assert(!memcmp(&zero,&services,sizeof(zero)));xrtClearError();
    assert(xl6TransferServicesProcessResidentV1(NULL,&services));assert(services.iSize==sizeof(services)&&services.iAbiTag==XN_TRANSFER_SERVICES_V1_TAG);
    XL6_CodeOwner code={sizeof(code),NULL,portableAcquire,portableRelease};XL6_CodeLease* lease=NULL;
    assert(xl6CodeLeaseAcquire(&code,&lease)==XL6_CODE_LEASE_ACQUIRED&&lease&&portableAcquires==1);
    xn_transfer_services_v1 borrowed={0};assert(xl6TransferServicesFromLeaseV1(lease,NULL,&borrowed));
    assert(!borrowed.iFlags&&borrowed.pCodeCapability==lease&&portableAcquires==1&&!portableReleases);
#if defined(_WIN32)
    methods(&services);lifetime(&services);graphAndCosts(&services);failures(&borrowed);assert(!portableReleases);
#else
    void* out=(void*)1;unsigned native=4;xn_transfer_native_owner_v1 own={0};
    assert(borrowed.wrapDataObject(&borrowed,&native,&own,&out)<0&&!out&&native==4);xrtClearError();
    out=(void*)1;assert(services.wrapDropSource(&services,&native,&own,&out)<0&&!out&&native==4);xrtClearError();
    out=(void*)1;assert(services.wrapDropTarget(&services,&native,&own,&out)<0&&!out&&native==4);xrtClearError();
#endif
    xl6CodeLeaseRelease(lease);assert(portableReleases==1);
    assert(!xrtGetError());puts("resident transfer capsule typed methods/lifetime/checked failure passed");return 0;
}
