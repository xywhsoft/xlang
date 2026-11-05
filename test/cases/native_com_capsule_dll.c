#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <oleidl.h>
#include <assert.h>
#include "test/cases/native_com_capsule_dll.h"
typedef struct DllData {IDataObject iface;CapsuleDllState* state;} DllData;
static HRESULT STDMETHODCALLTYPE query(IDataObject* self,REFIID iid,void** out)
{(void)self;(void)iid;if(out)*out=NULL;assert(!"publisher QI must never be exposed");return E_NOINTERFACE;}
static ULONG STDMETHODCALLTYPE add(IDataObject* self){(void)self;assert(!"unique owner must not use native AddRef");return 0;}
static ULONG STDMETHODCALLTYPE drop(IDataObject* self){(void)self;assert(!"native checked release required");return 0;}
static HRESULT STDMETHODCALLTYPE get(IDataObject* self,FORMATETC* f,STGMEDIUM* m){(void)self;(void)f;(void)m;return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE here(IDataObject* self,FORMATETC* f,STGMEDIUM* m){(void)self;(void)f;(void)m;return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE queryData(IDataObject* self,FORMATETC* format)
{
    (void)format;CapsuleDllState* state=((DllData*)self)->state;++state->calls;
    if(state->releaseInside){assert(IDataObject_Release((IDataObject*)state->resident)==1);assert(!state->releases);}
    ++state->tails;return S_OK;
}
static HRESULT STDMETHODCALLTYPE canonical(IDataObject* self,FORMATETC* i,FORMATETC* o){(void)self;(void)i;(void)o;return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE set(IDataObject* self,FORMATETC* f,STGMEDIUM* m,BOOL r){(void)self;(void)f;(void)m;(void)r;return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE formats(IDataObject* self,DWORD d,IEnumFORMATETC** out){(void)self;(void)d;if(out)*out=NULL;return E_NOTIMPL;}
static HRESULT STDMETHODCALLTYPE advise(IDataObject* self,FORMATETC* f,DWORD flags,IAdviseSink* sink,DWORD* connection){(void)self;(void)f;(void)flags;(void)sink;if(connection)*connection=0;return OLE_E_ADVISENOTSUPPORTED;}
static HRESULT STDMETHODCALLTYPE unadvise(IDataObject* self,DWORD connection){(void)self;(void)connection;return OLE_E_ADVISENOTSUPPORTED;}
static HRESULT STDMETHODCALLTYPE advices(IDataObject* self,IEnumSTATDATA** out){(void)self;if(out)*out=NULL;return OLE_E_ADVISENOTSUPPORTED;}
static const IDataObjectVtbl vtable={query,add,drop,get,here,queryData,canonical,set,formats,advise,unadvise,advices};
static bool ready(const void* raw){return ((const DllData*)raw)->state->ready;}
static bool trace(const void* raw,xn_transfer_ownership_visitor visitor,void* user){(void)raw;(void)visitor;(void)user;return true;}
static int releaseOwner(void* raw,bool* consumed)
{
    DllData* data=raw;CapsuleDllState* state=data->state;*consumed=false;
    if(!state->ready)return 1;
    ++state->releases;int status=state->releaseStatus;*consumed=true;
    assert(HeapFree(GetProcessHeap(),0,data));return status;
}
__declspec(dllexport) int capsule_dll_create(CapsuleDllState* state,void** output,xn_transfer_native_owner_v1* owner,uint64_t creator)
{
    *output=NULL;DllData* data=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*data));if(!data)return -1;
    data->iface.lpVtbl=&vtable;data->state=state;
    *owner=(xn_transfer_native_owner_v1){sizeof(*owner),XN_TRANSFER_NATIVE_OWNER_V1_TAG,creator,ready,trace,releaseOwner};
    *output=&data->iface;return 0;
}
