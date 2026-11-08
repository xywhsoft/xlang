/* Real thread/process lifecycle with an offline updater in a dedicated directory. */
#include "app/xide_app.h"
#include "ui/catalog_view.h"
#include "ui/association_view.h"
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
static int catalogReady,associationsReady;
static volatile LONG uiBlockingCalls,launches;
static DWORD uiThread;
static wchar_t fakeModule[32768];
static DWORD WINAPI moduleName(HMODULE module,LPWSTR text,DWORD size)
{(void)module;size_t length=wcslen(fakeModule);if(length>=size)return 0;wcscpy(text,fakeModule);return (DWORD)length;}
static DWORD WINAPI attributes(LPCWSTR file)
{if(GetCurrentThreadId()==uiThread)InterlockedIncrement(&uiBlockingCalls);return GetFileAttributesW(file);}
static BOOL WINAPI spawn(LPCWSTR app,LPWSTR command,LPSECURITY_ATTRIBUTES process,LPSECURITY_ATTRIBUTES thread,BOOL inherit,DWORD flags,LPVOID environment,LPCWSTR directory,LPSTARTUPINFOW startup,LPPROCESS_INFORMATION info)
{if(GetCurrentThreadId()==uiThread)InterlockedIncrement(&uiBlockingCalls);InterlockedIncrement(&launches);return CreateProcessW(app,command,process,thread,inherit,flags,environment,directory,startup,info);}
static int catalogIsReady(xide_app* a){(void)a;return catalogReady;}
static int associationsAreReady(xide_app* a){(void)a;return associationsReady;}
#define GetModuleFileNameW moduleName
#define GetFileAttributesW attributes
#define CreateProcessW spawn
#define xideCatalogViewSettled catalogIsReady
#define xideAssociationStartupSettled associationsAreReady
#include "../src/services/update_client.c"
#undef GetModuleFileNameW
#undef GetFileAttributesW
#undef CreateProcessW
#undef xideCatalogViewSettled
#undef xideAssociationStartupSettled
static int checks;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"startup_services_test:%d: %s\n",__LINE__,#e);failed=1;goto cleanup;}}while(0)
int main(int argc,char** argv)
{
    xide_app a={0};int failed=0;DWORD handlesBefore=0,handlesAfter=0;uiThread=GetCurrentThreadId();
    CHECK(argc==2);int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,fakeModule,32000);CHECK(n>0);wcscat(fakeModule,L"\\fixture.exe");
    a.hidden=1;xideUpdatePoll(&a);CHECK(!a.updater && !launches);
    a.hidden=0;xideUpdatePoll(&a);CHECK(a.updater && !a.updater->initialized && !launches);
    catalogReady=1;xideUpdatePoll(&a);CHECK(!a.updater->initialized && !launches);
    associationsReady=1;CHECK(GetProcessHandleCount(GetCurrentProcess(),&handlesBefore));xideUpdatePoll(&a);CHECK(a.updater->initialized && a.updater->launch);
    ULONGLONG until=GetTickCount64()+10000;
    while((a.updater->launch || a.updater->check) && GetTickCount64()<until){xideUpdatePoll(&a);Sleep(1);}
    CHECK(!a.updater->launch && !a.updater->check && launches==2 && !uiBlockingCalls && !a.updater->ready);
    CHECK(GetProcessHandleCount(GetCurrentProcess(),&handlesAfter));printf("updater first-use handles: %lu -> %lu\n",(unsigned long)handlesBefore,(unsigned long)handlesAfter);
    handlesBefore=handlesAfter;
    CHECK(check(&a,0,0));until=GetTickCount64()+10000;
    while((a.updater->launch || a.updater->check) && GetTickCount64()<until){xideUpdatePoll(&a);Sleep(1);}
    CHECK(!a.updater->launch && !a.updater->check && GetProcessHandleCount(GetCurrentProcess(),&handlesAfter));printf("updater repeated-use handles: %lu -> %lu\n",(unsigned long)handlesBefore,(unsigned long)handlesAfter);CHECK(handlesAfter==handlesBefore);
    xideUpdateDestroy(&a);CHECK(!a.updater);
    /* Cancellation may race either preparation or a just-created child. */
    for(int i=0;i<10;++i){CHECK(check(&a,0,0));xideUpdateDestroy(&a);}Sleep(250);
    CHECK(GetProcessHandleCount(GetCurrentProcess(),&handlesAfter) && handlesAfter==handlesBefore && !uiBlockingCalls);
cleanup:
    xideUpdateDestroy(&a);
    if(!failed)printf("startup_services_test passed: %d checks; catalog -> associations -> updater, offline child pair, worker-only file/process work, cancellation and exact handle balance\n",checks);
    return failed;
}
