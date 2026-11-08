/* Real save, watch worker and compiler-process gates; no timing-only race test.
 * Include the coordinator to observe its private queue without consuming it. */
#include "services/file_sync.h"
static xide_watch_event delayed;
static int deliverDelayed;
static int takeWatchEvent(xide_file_watch*,xide_watch_event*);
#define xideFileWatchTake takeWatchEvent
#ifndef XIDE_TEST_FILE_SYNC_SOURCE
#define XIDE_TEST_FILE_SYNC_SOURCE "../src/services/file_sync.c"
#endif
#include XIDE_TEST_FILE_SYNC_SOURCE
#undef xideFileWatchTake
#include "ui/task_view.h"
#include "platform/settings.h"
#include "theme/theme.h"
#ifndef XIDE_TEST_HELP_TITLE
#define XIDE_TEST_HELP_TITLE "集成帮助"
#endif

#define CHECK(e) do { ++checks; if(!(e)){fprintf(stderr,"task_save_watch_test:%d: %s; %s; task=%s\n",__LINE__,#e,error.message,info.error);failed=1;goto cleanup;} } while(0)
static int takeWatchEvent(xide_file_watch* watch,xide_watch_event* event)
{
    if(deliverDelayed){*event=delayed;memset(&delayed,0,sizeof(delayed));deliverDelayed=0;return 1;}
    return xideFileWatchTake(watch,event);
}
static int writeText(const char* path,const char* text)
{return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int compilerHelper(int argc,char** argv)
{
    if(argc==2 && !strcmp(argv[1],"--help")){puts("xl build <file.xl> -o <output>");return 0;}
    if(argc<5 || strcmp(argv[1],"build"))return -1;
    size_t size=0;char* text=(char*)xrtFileReadAll(argv[2],&size);if(!text)return 2;
    int valid=0;for(size_t i=0;i+9<=size;++i)if(!memcmp(text+i,"FAKE_GATE",9)){valid=1;break;}
    xrtFree(text);if(!valid)return 3;
    wchar_t name[96];DWORD n=GetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",name,96);HANDLE gate=n && n<96 ? OpenEventW(SYNCHRONIZE,FALSE,name):NULL;if(!gate)return 4;
    puts("BUILD_GATE_WAITING");fflush(stdout);DWORD waited=WaitForSingleObject(gate,30000);CloseHandle(gate);if(waited!=WAIT_OBJECT_0)return 5;
    char* directory=xidePathDirectory(argv[0]);char* program=directory ? xidePathResolve(directory,"xide_task_program.exe"):NULL;
    int okay=program && xrtFileCopy(program,argv[4],true);free(directory);free(program);return okay?0:6;
}
static int ready(xide_app* app)
{
    uint64_t end=GetTickCount64()+10000;
    do{xideEditorPoll(app);xideFileSyncPoll(app);if(xideFileSyncSettled(app))return 1;Sleep(5);}while(GetTickCount64()<end);
    return 0;
}
static int queued(xide_app* app)
{
    uint64_t end=GetTickCount64()+10000;
    do{xide_watch_stats s={0};xideFileWatchStats(app->fileSync->watch,&s);if(s.queued)return 1;Sleep(5);}while(GetTickCount64()<end);
    return 0;
}
static int gateWaiting(xide_task* task)
{
    uint64_t end=GetTickCount64()+10000;
    do{char log[8192]={0};uint64_t cursor=0,dropped=0;xideTaskReadLog(task,&cursor,log,sizeof(log)-1,&dropped);if(strstr(log,"BUILD_GATE_WAITING"))return 1;xide_task_info info={0};xideTaskInfo(task,&info);if(info.state>=XIDE_TASK_SUCCEEDED){fprintf(stderr,"gate failed: %s\n%s\n",info.error,log);return 0;}Sleep(5);}while(GetTickCount64()<end);
    return 0;
}
static int finished(xide_app* app,xide_task* task,xide_task_info* info)
{
    uint64_t end=GetTickCount64()+10000;
    do{xideTasksPoll(app->tasks);xideTaskInfo(task,info);if(info->state>=XIDE_TASK_SUCCEEDED)return 1;Sleep(5);}while(GetTickCount64()<end);
    return 0;
}
static int invalidated(xide_app* app,xide_task* task)
{
    uint64_t end=GetTickCount64()+10000;
    do{xideFileSyncPoll(app);xide_task_info info={0};xideTaskInfo(task,&info);if(info.invalidated)return 1;if(info.state>=XIDE_TASK_SUCCEEDED)return 0;Sleep(5);}while(GetTickCount64()<end);
    return 0;
}
int main(int argc,char** argv)
{
    int helper=compilerHelper(argc,argv);if(helper>=0)return helper;
    int failed=0,checks=0;xide_app app={0};xide_model_error error={0};xide_task_info info={0};HANDLE gate=NULL;
    char *root=NULL,*path=NULL,*compiler=NULL;wchar_t gateName[96];
    CHECK(argc==3);root=xidePathResolve(argv[1],"save-watch");CHECK(root && xrtDirCreateAll(root));
    path=xidePathResolve(root,"main.xl");compiler=copy(argv[2]);CHECK(path && compiler && writeText(path,"// FAKE_GATE initial\n"));
    swprintf(gateName,96,L"Local\\XlangSaveWatch-%lu",GetCurrentProcessId());gate=CreateEventW(NULL,TRUE,FALSE,gateName);CHECK(gate && SetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",gateName));
    app.hidden=1;app.noSession=1;app.viewportWidth=1280;app.viewportHeight=800;
    CHECK(xuiCreate(&app.context)==XUI_OK && ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(app.context,1280,800)==XUI_OK && xideWorkbenchCreate(&app)==XUI_OK);
    CHECK(!strcmp(xideActionFind(XIDE_ACTION_VIEW_HELP)->title,XIDE_TEST_HELP_TITLE));
    xui_dock_window_info_t help={.iSize=sizeof(help)};CHECK(xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_HELP],&help)==XUI_OK && !strcmp(help.sTitle,XIDE_TEST_HELP_TITLE));
    xideTaskViewAdoptCompiler(&app,compiler);compiler=NULL;
    CHECK(xideEditorOpen(&app,path,&error) && xideFileSyncStart(&app,&error) && ready(&app));
    xide_document* document=app.activeDocument;
    for(int round=0;round<8;++round){
        /* Keep a real old snapshot until after the next save succeeds. */
        xideFileWatchRescan(app.fileSync->watch);CHECK(queued(&app));
        CHECK(xideFileWatchTake(app.fileSync->watch,&delayed));
        char source[128];snprintf(source,sizeof(source),"// FAKE_GATE saved revision %d\n",round);
        CHECK(ResetEvent(gate) && xuiCodeEditSetText(document->editor,source)==XUI_OK);xideEditorPoll(&app);CHECK(document->dirty);
        CHECK(xideTaskViewAction(&app,XIDE_ACTION_BUILD_FILE));xide_task* task=xideTaskViewSelected(&app);
        CHECK(task);CHECK(!document->dirty);CHECK(gateWaiting(task));CHECK(queued(&app));
        deliverDelayed=1;xideFileSyncPoll(&app);xideTaskInfo(task,&info);CHECK(!info.invalidated);
        CHECK(!strcmp(xuiCodeEditGetText(document->editor),source) && !document->dirty);
        /* An identical atomic rewrite changes identity, but not build input. */
        CHECK(writeText(path,source) && queued(&app));xideFileSyncPoll(&app);xideTaskInfo(task,&info);
        if(info.invalidated)for(size_t i=0;i<app.fileSync->count;++i){subscription* t=&app.fileSync->targets[i];fprintf(stderr,"round=%d path=%s state=%d error=%u size=%llu hash=%llu baseline=%zu\n",round,t->path,t->last.state,t->last.error,(unsigned long long)t->last.size,(unsigned long long)t->last.hash,document->baseline.rawLength);}
        CHECK(!info.invalidated);
#ifndef XIDE_TEST_OLD_WATCH
        /* A temporary sharing failure is not evidence that bytes changed. */
        subscription* target=&app.fileSync->targets[0];
        delayed=(xide_watch_event){.epoch=app.fileSync->epoch,.token=target->target.token,.state=XIDE_WATCH_UNREADABLE,.error=ERROR_SHARING_VIOLATION,.observedAt=GetTickCount64(),.path=copy(path)};
        CHECK(delayed.path);deliverDelayed=1;xideFileSyncPoll(&app);xideTaskInfo(task,&info);CHECK(!info.invalidated);
        xideFileWatchRescan(app.fileSync->watch);
#endif
        CHECK(SetEvent(gate) && finished(&app,task,&info) && info.state==XIDE_TASK_SUCCEEDED && info.built && !info.invalidated && ready(&app));
    }
    for(int mode=0;mode<3;++mode){
        CHECK(ResetEvent(gate) && xideTaskViewAction(&app,XIDE_ACTION_BUILD_FILE));xide_task* task=xideTaskViewSelected(&app);CHECK(task && gateWaiting(task));
        if(mode==0){CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(document->editor),0,"// unsaved edit\n")==XUI_OK);xideEditorPoll(&app);}
        else if(mode==1){CHECK(writeText(path,"// FAKE_GATE actual external change\n") && queued(&app));xideFileSyncPoll(&app);}
        else{wchar_t* name=xidePathNativeWide(path);CHECK(name);int deleted=DeleteFileW(name);free(name);CHECK(deleted && queued(&app));xideFileSyncPoll(&app);}
        CHECK(invalidated(&app,task));
        CHECK(SetEvent(gate) && finished(&app,task,&info) && info.invalidated && info.state==XIDE_TASK_FAILED && !info.built);
        if(mode==0)CHECK(xideDocumentSave(document,NULL,0,&error));
        CHECK(ready(&app));
    }
    printf("task_save_watch_test passed: %d checks; 8 immediate save/build rounds, delayed old snapshots, identical replacement, sharing failures, real dirty edit/external change/deletion protection; integrated-help title.\n",checks);
cleanup:
    if(gate)SetEvent(gate);
    xideFileWatchEventClear(&delayed);xideAppDestroy(&app);if(gate)CloseHandle(gate);SetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",NULL);free(root);free(path);free(compiler);return failed;
}
