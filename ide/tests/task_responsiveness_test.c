#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "platform/path.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture {const char* directory;const char* program;HDESK desktop;HANDLE ready,release;int failed,checks;} fixture;
#define CHECK(e) do {++f->checks;if(!(e)){fprintf(stderr,"task_responsiveness_test:%d: %s\n",__LINE__,#e);f->failed=1;goto cleanup;}}while(0)
static double now(void) {LARGE_INTEGER n,f;QueryPerformanceCounter(&n);QueryPerformanceFrequency(&f);return (double)n.QuadPart/f.QuadPart;}
static int frame(xide_app* app) {return xideAppFrame(app)==XUI_OK && !app->frameError;}
static int emit(const void* data,size_t size) {while(size){DWORD n=0;if(!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),data,(DWORD)size,&n,NULL)||!n)return 0;data=(const char*)data+n;size-=n;}return 1;}

/* A local compiler fixture: the parent keeps it blocked while exercising the
   actual UI. No timing race decides whether compilation is still in flight. */
static int compiler(int argc,char** argv)
{
    if(argc==2 && !strcmp(argv[1],"--help")) {const char* help="xl build <file> -o <output>\n";return emit(help,strlen(help))?0:2;}
    if(argc<5 || strcmp(argv[1],"build")) return -1;
    wchar_t readyName[128],releaseName[128],quiet[8]={0};
    if(!GetEnvironmentVariableW(L"XIDE_BUILD_UI_READY",readyName,128) || !GetEnvironmentVariableW(L"XIDE_BUILD_UI_RELEASE",releaseName,128))return 3;
    HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,readyName),release=OpenEventW(SYNCHRONIZE,FALSE,releaseName);
    if(!ready || !release)return 4;
    GetEnvironmentVariableW(L"XIDE_BUILD_UI_QUIET",quiet,8);
    if(quiet[0]!=L'1') {
        char bytes[16384];const char* line="Compiling source and resolving symbols: UI input remains available while the compiler is busy.\n";
        size_t n=strlen(line);for(size_t i=0;i<sizeof(bytes);++i)bytes[i]=line[i%n];
        for(int i=0;i<40;++i)if(!emit(bytes,sizeof(bytes)))return 5;
    }
    const char* marker="\nBACKGROUND_COMPILER_WAITING\n";if(!emit(marker,strlen(marker)))return 6;
    SetEvent(ready);DWORD waited=WaitForSingleObject(release,30000);CloseHandle(ready);CloseHandle(release);if(waited!=WAIT_OBJECT_0)return 7;
    wchar_t self[32768];if(!GetModuleFileNameW(NULL,self,32768))return 8;
    wchar_t* target=xidePathNativeWide(argv[4]);int okay=target && CopyFileW(self,target,FALSE);free(target);return okay?0:9;
}
static int waitDone(xide_app* app,xide_task* task,xide_task_info* info)
{
    ULONGLONG start=GetTickCount64();
    while(GetTickCount64()-start<10000) {
        if(!frame(app))return 0;
        xideTaskInfo(task,info);
        if(info->state>=XIDE_TASK_SUCCEEDED && xideTaskViewSettled(app))return 1;
        Sleep(1);
    }
    return 0;
}
static int run(void* user)
{
    fixture* f=user;xide_app app={0};xide_model_error error={0};xide_task_info info={0};
    char* settings=xidePathResolve(f->directory,"settings.json"),*source=xidePathResolve(f->directory,"main.xl"),*image=xidePathResolve(f->directory,"background-build.png");
    const char* body="func main(...) as int\n return 0\nend\n";
    CHECK(settings && source && image && xrtFileWriteAtomic(source,(xbytesview){(const unsigned char*)body,strlen(body)}));
    app.hidden=1;app.noSession=1;app.disableAutomaticDiagnostics=1;app.dpiOverride=1;app.maxFrames=100000;app.settingsPath=settings;app.compilerPath=f->program;app.initialFile=source;
    CHECK(xideAppCreate(&app)==XUI_OK && frame(&app));xide_document* entry=app.activeDocument;CHECK(entry);
    xui_widget output=app.panels[XIDE_OUTPUT];CHECK(xuiWidgetGetType(output)==xuiCodeEditGetType(app.context) && xuiCodeEditIsReadonly(output) && xuiCodeEditGetWordWrap(output));
    double began=now();CHECK(xideTaskViewAction(&app,XIDE_ACTION_BUILD_FILE));double submitMs=(now()-began)*1000;
    xide_task* task=xideTaskViewSelected(&app);CHECK(task && submitMs<1000);
    xide_document* draft=xideDocumentNew(&app.documents,"text",&error);CHECK(draft && xideEditorShow(&app,draft)==XUI_OK && xuiSetFocusWidget(app.context,draft->editor)==XUI_OK);
    CHECK(xuiInputTextEx(app.context,'Q',NULL)==XUI_OK && frame(&app) && !strcmp(xuiCodeEditGetText(draft->editor),"Q"));
    double maxMs=0,sumMs=0;int frames=0;ULONGLONG start=GetTickCount64();
    while(GetTickCount64()-start<10000) {
        began=now();CHECK(frame(&app));double ms=(now()-began)*1000;sumMs+=ms;if(ms>maxMs)maxMs=ms;++frames;
        xideTaskInfo(task,&info);CHECK(info.state<XIDE_TASK_SUCCEEDED);
        if(WaitForSingleObject(f->ready,0)==WAIT_OBJECT_0 && strstr(xuiCodeEditGetText(output),"BACKGROUND_COMPILER_WAITING"))break;
        Sleep(1);
    }
    CHECK(frames && WaitForSingleObject(f->ready,0)==WAIT_OBJECT_0 && WaitForSingleObject(f->release,0)==WAIT_TIMEOUT);
    CHECK(strlen(xuiCodeEditGetText(output))>128u*1024u && strlen(xuiCodeEditGetText(output))<256u*1024u+512 && strstr(xuiCodeEditGetText(output),"显示已省略"));
    xui_rect_t rect=xuiWidgetGetWorldRect(output);CHECK(rect.fW>100 && rect.fH>100 && xuiWidgetGetEffectiveVisible(output));
    CHECK(xuiSetFocusWidget(app.context,output)==XUI_OK && xuiCodeSelectionSetRange(xuiCodeEditGetSelection(output),xuiCodeEditGetDocument(output),0,30)==XUI_OK);
    uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(output));
    CHECK(xuiInputTextEx(app.context,'X',NULL)==XUI_OK && frame(&app) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(output))==version);
    CHECK(xideAppCapture(&app,image)==XGE_OK);
    began=now();CHECK(xideTaskViewAction(&app,XIDE_ACTION_STOP));double stopMs=(now()-began)*1000;
    CHECK(stopMs<1000 && waitDone(&app,task,&info) && info.state==XIDE_TASK_CANCELLED && !info.built);
    CHECK(xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(output))==version); /* selection freezes the visible snapshot */
    CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(output),xuiCodeEditGetDocument(output),0,0)==XUI_OK && frame(&app));
    CHECK(strstr(xuiCodeEditGetText(output),"cancelled") && !strcmp(xuiCodeEditGetText(draft->editor),"Q"));
    ResetEvent(f->ready);CHECK(SetEnvironmentVariableW(L"XIDE_BUILD_UI_QUIET",L"1") && xideEditorShow(&app,entry)==XUI_OK);
    CHECK(xideTaskViewAction(&app,XIDE_ACTION_BUILD_FILE));task=xideTaskViewSelected(&app);CHECK(task);
    start=GetTickCount64();while(WaitForSingleObject(f->ready,0)!=WAIT_OBJECT_0 && GetTickCount64()-start<10000){CHECK(frame(&app));Sleep(1);}
    CHECK(WaitForSingleObject(f->ready,0)==WAIT_OBJECT_0 && SetEvent(f->release) && waitDone(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built && !info.ran);
    CHECK(strstr(xuiCodeEditGetText(output),"Build succeeded") && xidePathExists(xideTaskOutput(task)));
    printf("task_responsiveness_test passed: %d checks, submit %.3f ms, stop %.3f ms, %d real frames during blocked compiler, mean %.3f ms, max %.3f ms; bounded visible log, draft input, readonly selection, cancellation and fresh-artifact success\n",f->checks,submitMs,stopMs,frames,sumMs/frames,maxMs);fflush(stdout);
cleanup:
    SetEvent(f->release);xideAppDestroy(&app);free(settings);free(source);free(image);xgeSetQuitRequestCallback(NULL,NULL);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1280;d.iHeight=900;d.sTitle="Background compiler UI verification";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,f);xgeUnit();return result==XGE_OK&&!f->failed?0:1;
}
int main(int argc,char** argv)
{
    int child=compiler(argc,argv);if(child>=0)return child;if(argc!=2 || !xrtDirCreateAll(argv[1]))return 2;
    fixture f={.directory=argv[1],.program=argv[0]};wchar_t name[96],ready[96],release[96];DWORD id=GetCurrentProcessId();
    swprintf(name,96,L"BuildUI-%lu",id);swprintf(ready,96,L"Local\\BuildUIReady-%lu",id);swprintf(release,96,L"Local\\BuildUIRelease-%lu",id);
    f.ready=CreateEventW(NULL,TRUE,FALSE,ready);f.release=CreateEventW(NULL,TRUE,FALSE,release);if(!f.ready || !f.release)return 3;
    if(!SetEnvironmentVariableW(L"XIDE_BUILD_UI_READY",ready) || !SetEnvironmentVariableW(L"XIDE_BUILD_UI_RELEASE",release))return 4;
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 5;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=6;if(thread){if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0)ExitProcess(7);GetExitCodeThread(thread,&code);CloseHandle(thread);}
    CloseDesktop(f.desktop);CloseHandle(f.ready);CloseHandle(f.release);return (int)code;
}
