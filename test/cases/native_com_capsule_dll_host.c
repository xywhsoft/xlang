#define XRT_MODULE_ONCE
#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_native_com.h"
#include "src/runtime/xl6_foreign_view.h"
#include "src/runtime/xl6_callback_failure.h"
#include "test/cases/native_com_capsule_dll.h"
#include <assert.h>
#include <stdio.h>
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <oleidl.h>
typedef struct Host {const char* path;HMODULE acquired;unsigned acquires,releases;bool closed;} Host;
static int acquire(void* context)
{Host* host=context;if(host->closed)return 0;assert(!host->acquired);host->acquired=LoadLibraryA(host->path);assert(host->acquired);++host->acquires;return 1;}
static void release(void* context)
{Host* host=context;HMODULE acquired=host->acquired;host->acquired=NULL;++host->releases;assert(FreeLibrary(acquired));}
static DWORD WINAPI workerRelease(void* value)
{assert(!IDataObject_Release((IDataObject*)value));return 0;}
static void trial(const char* path,bool worker,bool releaseInside,int failure)
{
    HMODULE module=LoadLibraryA(path);assert(module);FARPROC address=GetProcAddress(module,"capsule_dll_create");assert(address);
    CapsuleDllCreate create=NULL;_Static_assert(sizeof(create)==sizeof(address),"Win loader function address ABI");memcpy(&create,&address,sizeof(create));
    Host host={.path=path};XL6_CodeOwner owner={sizeof(owner),&host,acquire,release};XL6_CodeLease* lease=NULL;
    assert(xl6CodeLeaseAcquire(&owner,&lease)==XL6_CODE_LEASE_ACQUIRED&&lease&&host.acquires==1);
    xn_transfer_services_v1 services={0};assert(xl6TransferServicesFromLeaseV1(lease,NULL,&services));
    CapsuleDllState state={.ready=releaseInside,.releaseInside=releaseInside,.releaseStatus=failure};void* native=NULL;xn_transfer_native_owner_v1 duty={0};
    assert(!create(&state,&native,&duty,xrtThreadCurrentId()));void* resident=NULL;
    /* Continuing an accepted lease after admission closes is intentional. */
    host.closed=true;assert(!services.wrapDataObject(&services,native,&duty,&resident));state.resident=resident;
    xl6CodeLeaseRelease(lease);assert(FreeLibrary(module));assert(GetModuleHandleA(path)&&!host.releases);
    if(releaseInside){FORMATETC format={0};assert(IDataObject_QueryGetData((IDataObject*)resident,&format)==S_OK);assert(state.calls==1&&state.tails==1);}
    else if(worker){HANDLE thread=CreateThread(NULL,0,workerRelease,resident,0,NULL);assert(thread);assert(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);CloseHandle(thread);}
    else assert(!IDataObject_Release((IDataObject*)resident));
    if(!releaseInside){
        assert(!state.releases&&!host.releases&&GetModuleHandleA(path));assert(xl6ForeignViewDrainV1());
        assert(!state.releases&&GetModuleHandleA(path));state.ready=true;
        if(failure){assert(!xl6ForeignViewDrainV1());xrtClearError();}else assert(xl6ForeignViewDrainV1());
    }
    assert(state.releases==1&&host.releases==1&&!GetModuleHandleA(path)&&!xl6ForeignViewPendingV1(0));
}
int main(int argc,char** argv)
{
    assert(argc==2&&xrtMemDebugEnable(true));trial(argv[1],false,false,0);trial(argv[1],true,false,0);trial(argv[1],false,true,0);trial(argv[1],true,false,-16);
    puts("actual DLL: accepted lease survives close, worker last Release waits, native tail precedes unload, checked failure consumes once");return 0;
}
