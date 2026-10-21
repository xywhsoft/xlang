#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "services/session.h"
#include "ui/workspace_view.h"
#include "platform/window_state.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"window_test phase=%d line=%d: %s; %s\n",fixture->phase,__LINE__,#e,error.message); fixture->failed=1; goto cleanup; } } while(0)
typedef struct window_fixture {
    xide_app app;
    xide_window_state state;
    xide_window_geometry normal;
    char *root,*storage,*file,*image,*preferences,*metadata;
    int phase,started,failed;
    WNDPROC original;
    HDESK desktop;
    wchar_t desktopName[100];
    const char* directory;
} window_fixture;
static xstrview sv(const char* text) { return (xstrview){text,strlen(text)}; }
static xvalue* get(const xvalue* root,const char* key) { return xrtValueObjectGet(root,sv(key)); }
static int same(const xide_window_geometry* a,const xide_window_geometry* b)
{ return abs(a->x-b->x)<=2 && abs(a->y-b->y)<=2 && abs(a->width-b->width)<=2 && abs(a->height-b->height)<=2; }
static WNDPROC outerPrevious;
static LRESULT CALLBACK outerProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{ return CallWindowProcW(outerPrevious,window,message,wParam,lParam); }
static int subclassLifetime(HWND window,const char* directory)
{
    xide_window_state state={0}; xide_model_error error={0};
    WNDPROC original=(WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC);
    if(!xideWindowStateOpen(&state,window,directory,0,NULL,&error)) return 0;
    outerPrevious=(WNDPROC)SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)outerProcedure);
    xideWindowStateClose(&state);
    SendMessageW(window,WM_NULL,0,0);
    int okay=xideWindowStateOpen(&state,window,directory,0,NULL,&error);
    SendMessageW(window,WM_NULL,0,0);
    xideWindowStateClose(&state);
    okay=okay && (WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC)==outerProcedure;
    SetWindowLongPtrW(window,GWLP_WNDPROC,(LONG_PTR)outerPrevious);
    okay=xideWindowStateOpen(&state,window,directory,0,NULL,&error) && okay;
    xideWindowStateClose(&state);
    return okay && (WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC)==original;
}
static int newApp(window_fixture* fixture,int explicitSize)
{
    memset(&fixture->app,0,sizeof(fixture->app)); fixture->app.maxFrames=10000; fixture->app.sessionRoot=fixture->storage;
    fixture->app.settingsPath=fixture->preferences; fixture->app.windowSizeExplicit=explicitSize; return xideAppCreate(&fixture->app)==XUI_OK;
}
static int captureImage(xide_app* app,const char* path)
{
    size_t bytes=(size_t)app->viewportWidth*(size_t)app->viewportHeight*4; unsigned char* pixels=malloc(bytes); if(!pixels) return 0;
    int result=app->proxy.surfaceReadRGBA(&app->proxy,app->target,pixels,app->viewportWidth*4);
    if(result==XUI_OK) result=xgeImageSavePNG(path,app->viewportWidth,app->viewportHeight,pixels,app->viewportWidth*4);
    free(pixels); return result==XGE_OK;
}
static int syntheticPlan(void)
{
    xide_window_geometry geometry={0}; geometry.x=80; geometry.y=60; geometry.width=900; geometry.height=600;
    for(int dpi=96;dpi<=192;dpi+=24) {
        xide_window_rect rectangle={0},work={-2560,40,2560,1400};
        if(!xideWindowPlan(&geometry,work,dpi,640,480,&rectangle) || rectangle.x!=work.x+MulDiv(80,dpi,96) || rectangle.y!=work.y+MulDiv(60,dpi,96) || rectangle.width!=MulDiv(900,dpi,96) || rectangle.height!=MulDiv(600,dpi,96)) return 0;
    }
    xide_window_rect rectangle={0},work={40,32,800,600}; geometry.x=-100000; geometry.y=100000; geometry.width=3000; geometry.height=2000;
    if(!xideWindowPlan(&geometry,work,144,656,519,&rectangle) || rectangle.x!=work.x || rectangle.y!=work.y || rectangle.width!=800 || rectangle.height!=600) return 0;
    geometry.width=100; geometry.height=100; geometry.x=geometry.y=0;
    if(!xideWindowPlan(&geometry,work,96,656,519,&rectangle) || rectangle.width!=656 || rectangle.height!=519) return 0;
    return !xideWindowPlan(&geometry,work,0,640,480,&rectangle);
}
/* This worker owns the live parent HWND. Child window initialization can send
   synchronous window messages to it, so a plain process wait can stall both.
   Pump messages while retaining the original absolute child-process deadline. */
static DWORD waitNativeChild(HANDLE process,DWORD timeout)
{
    ULONGLONG deadline=GetTickCount64()+timeout;
    for(;;) {
        ULONGLONG now=GetTickCount64();
        if(now>=deadline) return WaitForSingleObject(process,0);
        DWORD waited=MsgWaitForMultipleObjectsEx(1,&process,(DWORD)(deadline-now),QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(waited!=WAIT_OBJECT_0+1) return waited;
        MSG message;
        while(PeekMessageW(&message,NULL,0,0,PM_REMOVE)) {
            if(message.message==WM_QUIT) { PostQuitMessage((int)message.wParam); SetLastError(ERROR_CANCELLED); return WAIT_FAILED; }
            TranslateMessage(&message); DispatchMessageW(&message);
            if(GetTickCount64()>=deadline) break;
        }
    }
}
static int nativeProcess(window_fixture* fixture,const char* directory,const char* image,int explicitSize)
{
    wchar_t executable[32768],storage[32768],preferences[32768],picture[32768],command[32768];
    DWORD length=GetModuleFileNameW(NULL,executable,32768); if(!length || length>=32768) return 0;
    wchar_t* separator=wcsrchr(executable,L'\\'); if(!separator || separator-executable+32>=32768) return 0;
    wcscpy(separator+1,L"xlang_ide_console.exe");
    if(!MultiByteToWideChar(CP_UTF8,0,directory,-1,storage,32768) || !MultiByteToWideChar(CP_UTF8,0,fixture->preferences,-1,preferences,32768) ||
       (image && !MultiByteToWideChar(CP_UTF8,0,image,-1,picture,32768))) return 0;
    int count=swprintf(command,32768,L"\"%ls\" --frames 3 --session-root \"%ls\" --settings \"%ls\" %ls %ls%ls%ls",executable,storage,preferences,
        explicitSize ? L"--size 900 600" : L"",image ? L"--screenshot \"" : L"",image ? picture : L"",image ? L"\"" : L"");
    if(count<0 || count>=32768) return 0;
    HANDLE job=CreateJobObjectW(NULL,NULL); if(!job) return 0;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={0}; limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) { CloseHandle(job); return 0; }
    wchar_t logPath[32768]; int logLength=swprintf(logPath,32768,L"%ls\\restart-%d.log",storage,explicitSize);
    if(logLength<0 || logLength>=32768) { CloseHandle(job); return 0; }
    SECURITY_ATTRIBUTES inherit={sizeof(inherit),NULL,TRUE}; HANDLE log=CreateFileW(logPath,GENERIC_WRITE,FILE_SHARE_READ,&inherit,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&inherit,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(log==INVALID_HANDLE_VALUE || input==INVALID_HANDLE_VALUE) { if(log!=INVALID_HANDLE_VALUE) CloseHandle(log); if(input!=INVALID_HANDLE_VALUE) CloseHandle(input); CloseHandle(job); return 0; }
    STARTUPINFOW startup={0}; startup.cb=sizeof(startup); startup.lpDesktop=fixture->desktopName; startup.dwFlags=STARTF_USESTDHANDLES; startup.hStdOutput=startup.hStdError=log; startup.hStdInput=input; PROCESS_INFORMATION process={0};
    if(!CreateProcessW(executable,command,NULL,NULL,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,NULL,NULL,&startup,&process)) { fprintf(stderr,"window child creation failed: Windows %lu\n",GetLastError()); CloseHandle(log); CloseHandle(input); CloseHandle(job); return 0; }
    CloseHandle(log); CloseHandle(input);
    int okay=AssignProcessToJobObject(job,process.hProcess) && ResumeThread(process.hThread)!=(DWORD)-1;
    if(!okay) TerminateProcess(process.hProcess,1);
    ULONGLONG waitStarted=GetTickCount64();
    DWORD waited=okay ? waitNativeChild(process.hProcess,15000) : WAIT_FAILED,code=1;
    okay=okay && waited==WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess,&code) && code==0;
    CloseHandle(process.hThread); CloseHandle(job); if(!okay) waitNativeChild(process.hProcess,3000); CloseHandle(process.hProcess);
    if(okay) printf("window child passed: original 15000 ms wait, %llu ms elapsed, explicit=%d\n",(unsigned long long)(GetTickCount64()-waitStarted),explicitSize);
    if(!okay) {
        fprintf(stderr,"window child failed: waited=%lu exit=%lu explicit=%d\n",waited,code,explicitSize);
        char* childLog=xidePathResolve(directory,explicitSize ? "restart-1.log" : "restart-0.log"); size_t logSize=0; unsigned char* raw=childLog ? xrtFileReadAllLimit(childLog,16384,&logSize) : NULL;
        if(raw) { fwrite(raw,1,logSize,stderr); } xrtFree(raw); free(childLog);
    }
    return okay;
}
static int pngSize(const char* image,int* width,int* height)
{
    size_t size=0; unsigned char* bytes=xrtFileReadAllLimit(image,16*1024*1024,&size);
    int okay=bytes && size>32 && !memcmp(bytes,"\x89PNG\r\n\x1a\n",8) && !memcmp(bytes+12,"IHDR",4);
    if(okay) { *width=(int)(((uint32_t)bytes[16]<<24)|((uint32_t)bytes[17]<<16)|((uint32_t)bytes[18]<<8)|bytes[19]);
        *height=(int)(((uint32_t)bytes[20]<<24)|((uint32_t)bytes[21]<<16)|((uint32_t)bytes[22]<<8)|bytes[23]); okay=*width>0 && *height>0; }
    xrtFree(bytes); return okay;
}
static int processRestart(window_fixture* fixture)
{
    char *directory=xidePathResolve(fixture->root,"中文 进程重启"),*file=directory ? xidePathResolve(directory,"window.json") : NULL;
    char *maxImage=xidePathResolve(fixture->root,"native-window-process-maximized.png"),*sizeImage=xidePathResolve(fixture->root,"native-window-process-size.png");
    xide_state_file state={0}; xide_model_error error={0}; xvalue* seed=NULL; xide_window_geometry expected={0},actual={0}; int width=0,height=0,okay=0;
    if(!directory || !file || !maxImage || !sizeImage || !xrtDirCreateAll(directory) || !xideStateOpen(&state,fixture->file,"xlang-ide-window-1",&error)) goto cleanup;
    seed=xrtValueDeepClone(state.source); if(!seed || !xideWindowGeometryRead(get(seed,"placement"),&expected)) goto cleanup;
    expected.maximized=1;
    if(!xrtValueObjectSetNew(get(seed,"placement"),sv("maximized"),xrtValueBool(true))) goto cleanup;
    xideStateClear(&state); if(!xideStateOpen(&state,file,"xlang-ide-window-1",&error) || !xideStateWrite(&state,seed,&error)) goto cleanup;
    if(!nativeProcess(fixture,directory,maxImage,0) || !pngSize(maxImage,&width,&height)) goto cleanup;
    xideStateClear(&state); if(!xideStateOpen(&state,file,"xlang-ide-window-1",&error) || !xideWindowGeometryRead(get(state.source,"placement"),&actual) || !actual.maximized || !same(&actual,&expected)) goto cleanup;
    if(!nativeProcess(fixture,directory,sizeImage,1) || !pngSize(sizeImage,&width,&height)) goto cleanup;
    xideStateClear(&state); if(!xideStateOpen(&state,file,"xlang-ide-window-1",&error) || !xideWindowGeometryRead(get(state.source,"placement"),&expected) || expected.maximized || width!=MulDiv(900,expected.dpi,96) || height!=MulDiv(600,expected.dpi,96)) goto cleanup;
    if(!nativeProcess(fixture,directory,NULL,0)) goto cleanup;
    xideStateClear(&state); if(!xideStateOpen(&state,file,"xlang-ide-window-1",&error) || !xideWindowGeometryRead(get(state.source,"placement"),&actual) || actual.maximized || !same(&actual,&expected)) goto cleanup;
    okay=1;
cleanup:
    if(!okay) fprintf(stderr,"window_test independent process restart failed: %s\n",error.message);
    xrtValueRelease(seed); xideStateClear(&state); free(directory); free(file); free(maxImage); free(sizeImage); return okay;
}
static int verify(void* user)
{
    window_fixture* fixture=user; xide_model_error error={0}; HWND window=(HWND)xgePlatformNativeHandle(); xide_window_geometry current={0}; xide_state_file stateFile={0}; xvalue* changed=NULL; HANDLE locked=INVALID_HANDLE_VALUE;
    fixture->started=1;
    if(fixture->phase==0) {
        char* id=xideCreateId("window"); fixture->root=id ? xidePathResolve(fixture->directory,id) : NULL; free(id); CHECK(fixture->root && xrtDirCreateAll(fixture->root));
        fixture->storage=xidePathResolve(fixture->root,"中文 窗口状态"); fixture->file=xidePathResolve(fixture->storage,"window.json"); fixture->image=xidePathResolve(fixture->root,"native-window-restored.png"); fixture->preferences=xidePathResolve(fixture->root,"preferences.json"); CHECK(fixture->storage && fixture->file && fixture->image && fixture->preferences);
        fixture->original=(WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC);
        CHECK(syntheticPlan() && subclassLifetime(window,fixture->storage) && newApp(fixture,0));
        CHECK(xideWindowCapture(window,&current,&error)); current.x=60; current.y=70; current.width=1016; current.height=689; current.maximized=0;
        CHECK(xideWindowRestore(window,&current,&error)); CHECK(xideWindowCapture(window,&fixture->normal,&error));
        MINMAXINFO limits={0}; SendMessageW(window,WM_GETMINMAXINFO,0,(LPARAM)&limits); RECT outer={0},client={0};
        CHECK(GetWindowRect(window,&outer) && GetClientRect(window,&client));
        CHECK(limits.ptMinTrackSize.x-(outer.right-outer.left-client.right)>=640 && limits.ptMinTrackSize.y-(outer.bottom-outer.top-client.bottom)>=480);
        CHECK(xideSessionSave(&fixture->app,0,&error) && xidePathExists(fixture->file));
        CHECK(xideStateOpen(&stateFile,fixture->file,"xlang-ide-window-1",&error)); changed=xrtValueDeepClone(stateFile.source); CHECK(changed);
        CHECK(xrtValueObjectSetNew(changed,sv("futureWindow"),xrtValueInt(7)) && xrtValueObjectSetNew(get(changed,"placement"),sv("futurePlacement"),xrtValueString(sv("keep"))) && xideStateWrite(&stateFile,changed,&error));
        xideAppDestroy(&fixture->app); CHECK((WNDPROC)GetWindowLongPtrW(window,GWLP_WNDPROC)==fixture->original);
        CHECK(SetWindowPos(window,NULL,current.workX+150,current.workY+180,800,600,SWP_NOZORDER|SWP_NOACTIVATE)); fixture->phase=1;
    } else if(fixture->phase==1) {
        CHECK(newApp(fixture,0) && xideWindowCapture(window,&current,&error) && same(&current,&fixture->normal));
        xide_workspace workspace={0}; char* first=xidePathResolve(fixture->root,"first.xlw"),*second=xidePathResolve(fixture->root,"second.xlw");
        int workspaces=first && second && xideWorkspaceCreate(&workspace,"窗口恢复工作空间",&error) && xideWorkspaceSave(&workspace,first,&error) && xideWorkspaceSave(&workspace,second,&error);
        xideWorkspaceClear(&workspace); CHECK(workspaces);
        int switched=xideWorkspaceOpen(&fixture->app,first,0,&error) && xideWindowCapture(window,&current,&error) && same(&current,&fixture->normal) &&
            xideWorkspaceOpen(&fixture->app,second,0,&error) && xideWindowCapture(window,&current,&error) && same(&current,&fixture->normal);
        free(first); free(second); CHECK(switched);
        for(int i=0;i<20;++i) CHECK(xideWindowRestore(window,&current,&error) && xideWindowCapture(window,&current,&error) && same(&current,&fixture->normal));
        fixture->phase=2;
    } else if(fixture->phase==2) {
        RECT client={0}; CHECK(GetClientRect(window,&client) && client.right==xgeGetWidth() && client.bottom==xgeGetHeight());
        ShowWindow(window,SW_SHOWMAXIMIZED); CHECK(IsZoomed(window) && xideWindowCapture(window,&current,&error) && current.maximized && same(&current,&fixture->normal));
        ShowWindow(window,SW_MINIMIZE); CHECK(IsIconic(window) && xideWindowCapture(window,&current,&error) && current.maximized && same(&current,&fixture->normal));
        CHECK(xideSessionSave(&fixture->app,0,&error)); ShowWindow(window,SW_RESTORE); CHECK(IsZoomed(window));
        xideAppDestroy(&fixture->app); ShowWindow(window,SW_RESTORE); CHECK(!IsZoomed(window));
        CHECK(SetWindowPos(window,NULL,current.workX+150,current.workY+180,800,600,SWP_NOZORDER|SWP_NOACTIVATE)); fixture->phase=3;
    } else if(fixture->phase==3) {
        CHECK(newApp(fixture,0) && IsZoomed(window) && xideWindowCapture(window,&current,&error) && current.maximized && same(&current,&fixture->normal)); fixture->phase=4;
    } else if(fixture->phase==4) {
        CHECK(xideAppFrame(&fixture->app)==XGE_OK && !fixture->app.frameError && captureImage(&fixture->app,fixture->image));
        CHECK(xideSessionSave(&fixture->app,0,&error)); xideAppDestroy(&fixture->app);
        /* TSF may have attached after our first app was created. Its outer
           subclass remains valid after the IDE state owner is destroyed. */
        CHECK(!GetPropW(window,L"XlangIDE.WindowState"));
        SendMessageW(window,WM_NULL,0,0);
        ShowWindow(window,SW_RESTORE); CHECK(!IsZoomed(window));
        CHECK(SetWindowPos(window,NULL,fixture->normal.workX+120,fixture->normal.workY+100,850,620,SWP_NOZORDER|SWP_NOACTIVATE));
        CHECK(xideWindowCapture(window,&current,&error)); xide_window_geometry explicitGeometry=current;
        CHECK(newApp(fixture,1) && xideWindowCapture(window,&current,&error) && same(&current,&explicitGeometry) && !current.maximized);
        xideAppDestroy(&fixture->app);
        CHECK(xideWindowStateOpen(&fixture->state,window,fixture->storage,1,xgeRenderRequest,&error));
        CHECK(xideWindowStateSave(&fixture->state,&error));
        CHECK(get(fixture->state.file.source,"futureWindow") && get(get(fixture->state.file.source,"placement"),"futurePlacement"));
        /* Hidden restoration never makes the window visible. */
        ShowWindow(window,SW_HIDE); current=fixture->normal; current.maximized=1;
        CHECK(xideWindowRestore(window,&current,&error) && !IsWindowVisible(window)); ShowWindow(window,SW_SHOWNOACTIVATE);
        /* Missing monitor and grossly offscreen data are clamped into a work area. */
        current=fixture->normal; strcpy(current.monitor,"missing-monitor"); current.x=-900000; current.y=900000; current.maximized=0;
        CHECK(xideWindowRestore(window,&current,&error)); RECT bounds={0}; MONITORINFO monitor={0}; monitor.cbSize=sizeof(monitor); CHECK(GetWindowRect(window,&bounds) && GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor));
        CHECK(bounds.left>=monitor.rcWork.left && bounds.top>=monitor.rcWork.top && bounds.right<=monitor.rcWork.right && bounds.bottom<=monitor.rcWork.bottom);
        CHECK(xideWindowStatePoll(&fixture->state,&error)); fixture->phase=5;
    } else if(fixture->phase==5) {
        CHECK(xideWindowStatePoll(&fixture->state,&error));
        if(fixture->state.due) return XGE_OK;
        CHECK(xideStateOpen(&stateFile,fixture->file,"xlang-ide-window-1",&error));
        CHECK(xideWindowGeometryRead(get(stateFile.source,"placement"),&current) && current.x>=0 && current.y>=0 && !current.maximized);
        changed=xrtValueDeepClone(stateFile.source); CHECK(changed && xrtValueObjectSetNew(changed,sv("external"),xrtValueBool(1)) && xideStateWrite(&stateFile,changed,&error));
        CHECK(!xideWindowStateSave(&fixture->state,&error) && error.code==XIDE_MODEL_CONFLICT);
        xideWindowStateClose(&fixture->state); CHECK(xideWindowStateOpen(&fixture->state,window,fixture->storage,0,xgeRenderRequest,&error));
        wchar_t path[32768]; CHECK(MultiByteToWideChar(CP_UTF8,0,fixture->file,-1,path,32768)); locked=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL); CHECK(locked!=INVALID_HANDLE_VALUE);
        int saved=xideWindowStateSave(&fixture->state,&error); CloseHandle(locked); locked=INVALID_HANDLE_VALUE; CHECK(!saved && error.code==XIDE_MODEL_IO);
        xideWindowStateClose(&fixture->state);
        CHECK(xrtFileWriteAtomic(fixture->file,(xbytesview){(const unsigned char*)"{broken",7}));
        CHECK(!xideWindowStateOpen(&fixture->state,window,fixture->storage,1,xgeRenderRequest,&error) && error.code==XIDE_MODEL_INVALID && !fixture->state.ready && xideWindowStateSave(&fixture->state,&error));
        xideWindowStateClose(&fixture->state); CHECK(newApp(fixture,0) && !fixture->app.frameError && xideSessionSave(&fixture->app,0,&error));
        size_t size=0; unsigned char* bytes=xrtFileReadAllLimit(fixture->file,256,&size); int unchanged=bytes && size==7 && !memcmp(bytes,"{broken",7); xrtFree(bytes); CHECK(unchanged);
        size_t serializedSize=0; char* serialized=xrtJsonStringify(stateFile.source,true,&serializedSize);
        int repaired=serialized && xrtFileWriteAtomic(fixture->file,(xbytesview){(const unsigned char*)serialized,serializedSize}); xrtFree(serialized); CHECK(repaired);
        fixture->metadata=xidePathResolve(xideSessionDirectory(&fixture->app),"session.json"); CHECK(fixture->metadata && xrtFileWriteAtomic(fixture->metadata,(xbytesview){(const unsigned char*)"{bad workspace",14}));
        xideAppDestroy(&fixture->app); CHECK(newApp(fixture,0) && xideWindowCapture(window,&current,&error));
        current.x=15; current.y=20; current.maximized=0;
        CHECK(xideWindowRestore(window,&current,&error) && xideWindowCapture(window,&fixture->normal,&error));
        xideSessionPoll(&fixture->app); fixture->phase=6;
    } else if(fixture->phase==6) {
        xideSessionPoll(&fixture->app); CHECK(xideStateOpen(&stateFile,fixture->file,"xlang-ide-window-1",&error) && xideWindowGeometryRead(get(stateFile.source,"placement"),&current));
        if(!same(&current,&fixture->normal)) {
            CHECK(xideAppFrame(&fixture->app)==XGE_OK && !fixture->app.frameError); xideStateClear(&stateFile); return XGE_OK;
        }
        size_t size=0; unsigned char* bytes=xrtFileReadAllLimit(fixture->metadata,256,&size); int unchanged=bytes && size==14 && !memcmp(bytes,"{bad workspace",14); xrtFree(bytes); CHECK(unchanged);
        fixture->phase=7;
    } else if(fixture->phase==7) {
        CHECK(xideAppFrame(&fixture->app)==XGE_OK && !fixture->app.frameError);
        CHECK(processRestart(fixture));
        puts("window_test passed: actual XGE on an isolated desktop, native normal/maximized/minimized restore, normal-size retention, explicit-size override, hidden preservation, missing/offscreen monitor repair, minimum client size, 96/120/144/168/192 DPI planning, negative monitor/taskbar origins, autosave independent of corrupt workspace state, workspace switching and repeated restore, unknown fields, file conflicts/locking, corrupt preservation, live IDE rendering, three independent IDE process starts with actual CLI size override, and native procedure cleanup.");
        goto cleanup;
    }
    if(fixture->app.context) CHECK(xideAppFrame(&fixture->app)==XGE_OK && !fixture->app.frameError);
    xrtValueRelease(changed); xideStateClear(&stateFile); return XGE_OK;
cleanup:
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    xrtValueRelease(changed); xideStateClear(&stateFile); xideWindowStateClose(&fixture->state); xideAppDestroy(&fixture->app); xgeSetQuitRequestCallback(NULL,NULL); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    window_fixture* fixture=user;
    if(!SetThreadDesktop(fixture->desktop)) { fprintf(stderr,"window_test: SetThreadDesktop failed %lu\n",GetLastError()); return 1; }
    xge_desc_t desc={0}; desc.iWidth=1000; desc.iHeight=650; desc.sTitle="xlang IDE isolated window verification"; desc.iFlags=XGE_INIT_WINDOW; desc.iRunMode=XGE_RUN_GAME_LOOP; desc.iTargetFPS=60;
    int result=xgeInit(&desc); if(result==XGE_OK) result=xgeRun(verify,fixture); xideAppDestroy(&fixture->app); xgeUnit();
    return result==XGE_OK && fixture->started && !fixture->failed && fixture->phase==7 ? 0 : 1;
}
int main(int argc,char** argv)
{
    if(argc!=2) return 2;
    window_fixture fixture={0}; fixture.directory=argv[1]; wchar_t name[100]; swprintf(name,100,L"XlangIDEWindowVerification-%lu",GetCurrentProcessId());
    wcscpy(fixture.desktopName,name);
    fixture.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);
    if(!fixture.desktop) { fprintf(stderr,"window_test: CreateDesktop failed %lu\n",GetLastError()); return 1; }
    HANDLE thread=CreateThread(NULL,0,worker,&fixture,0,NULL); DWORD result=1;
    if(thread) { DWORD waited=WaitForSingleObject(thread,60000); if(waited!=WAIT_OBJECT_0) { fputs("window_test: native verification timed out.\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&result); CloseHandle(thread); }
    CloseDesktop(fixture.desktop); free(fixture.root); free(fixture.storage); free(fixture.file); free(fixture.image); free(fixture.preferences); free(fixture.metadata); return (int)result;
}
