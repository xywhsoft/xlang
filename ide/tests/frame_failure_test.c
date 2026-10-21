#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "services/session.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int boundaryBegin(void);
static int boundaryEnd(void);
#define xgeBegin boundaryBegin
#define xgeEnd boundaryEnd
#include "app/xide_app.c"
#undef xgeBegin
#undef xgeEnd

typedef struct fixture {
    xide_app app;
    HDESK desktop;
    const char* directory;
    char *settings,*storage,*cId,*xlId,*metadata;
    HANDLE lock;
    uint64_t failedAt;
    int failed,started,checks,restore,previous,code,protectedStorage,noSession,boundary,inject;
} fixture;
static fixture* active;
static int boundaryBegin(void)
{ if(active && active->inject && active->boundary==1) { active->failedAt=GetTickCount64();return active->code; } return xgeBegin(); }
static int boundaryEnd(void)
{ if(active && active->inject && active->boundary==2) { active->failedAt=GetTickCount64();return active->code; } return xgeEnd(); }

#define CHECK(expr) do { ++f->checks; if(!(expr)) { fprintf(stderr,"frame_failure_test:%d: %s; %s\n",__LINE__,#expr,error.message); f->failed=1; goto cleanup; } } while(0)
static const char cOld[]="int previous_c = 1;\n";
static const char xlOld[]="func previous(...) as int\n    return 1\nend\n";
static const char cNew[]="int previous_c = 1;\n// 最新未保存 C 中文 😀\n";
static const char xlNew[]="func previous(...) as int\n    return 1\nend\n// 最新未保存 xlang 中文 😀\n";
static const char damaged[]="{ damaged external session bytes\n";
static int failedClear(xui_proxy proxy,xui_surface target,uint32_t color)
{ (void)proxy;(void)target;(void)color;active->failedAt=GetTickCount64();return active->code; }
static char* copy(const char* text)
{ size_t n=strlen(text)+1;char* result=malloc(n);if(result)memcpy(result,text,n);return result; }
static xide_document* findId(xide_app* app,const char* id)
{
    for(size_t i=0;i<app->documents.count;++i) {
        xide_document* d=app->documents.items[i];
        if(d->open && d->sessionId && !strcmp(d->sessionId,id))return d;
    }
    return NULL;
}
static int write(const char* path,const char* text)
{
    char* native=xidePathNativeUtf8(path);
    int okay=native && xrtFileWriteAtomic(native,(xbytesview){(const unsigned char*)text,strlen(text)});
    free(native);return okay;
}
static int matches(const char* path,const char* text)
{
    char* native=xidePathNativeUtf8(path);size_t n=0;
    unsigned char* bytes=native ? xrtFileReadAllLimit(native,1024,&n) : NULL;free(native);
    int okay=bytes && n==strlen(text) && !memcmp(bytes,text,n);xrtFree(bytes);return okay;
}
static int capture(fixture* f)
{
    char* path=xidePathResolve(f->directory,"native-restored-after-frame-failure.png");
    int okay=path && xideAppCapture(&f->app,path)==XGE_OK;free(path);return okay;
}
static int run(void* user)
{
    fixture* f=user;xide_app* app=&f->app;xide_model_error error={0};
    xide_document *c=NULL,*xl=NULL;uint64_t edited=0;int result=0;
    f->started=1;active=f;
    CHECK(xrtDirCreateAll(f->directory));
    f->settings=xidePathResolve(f->directory,"preferences.json");f->storage=xidePathResolve(f->directory,"sessions");
    CHECK(f->settings && f->storage);
    app->hidden=1;app->maxFrames=10000;app->dpiOverride=1;
    app->settingsPath=f->settings;app->sessionRoot=f->storage;app->noSession=f->noSession;
    CHECK(xideAppFrame(app)==XGE_OK && !app->frameError);
    if(f->restore) {
        c=findId(app,f->cId);xl=findId(app,f->xlId);CHECK(c && xl && c->dirty && xl->dirty);
        CHECK(!strcmp(xuiCodeEditGetLanguage(c->editor),"c") && !strcmp(xuiCodeEditGetLanguage(xl->editor),"xlang"));
        CHECK(!strcmp(xuiCodeEditGetText(c->editor),f->previous ? cOld : cNew));
        CHECK(!strcmp(xuiCodeEditGetText(xl->editor),f->previous ? xlOld : xlNew));
        if(!f->previous) {
            xui_code_selection_t cs={0},xs={0};
            CHECK(xuiCodeSelectionGetState(xuiCodeEditGetSelection(c->editor),&cs)==XUI_OK && xuiCodeSelectionGetState(xuiCodeEditGetSelection(xl->editor),&xs)==XUI_OK);
            CHECK(cs.iCaretOffset==4 && cs.iAnchorOffset==4 && xs.iCaretOffset==5 && xs.iAnchorOffset==5);
        }
        CHECK(capture(f));
        printf("frame_failure_test restart passed: %d checks; restoredPrevious=%d\n",f->checks,f->previous);fflush(stdout);
        goto cleanup;
    }
    c=xideDocumentNew(&app->documents,"c",&error);xl=xideDocumentNew(&app->documents,"xlang",&error);CHECK(c && xl);
    CHECK(xideEditorShow(app,c)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),0,cOld)==XUI_OK);
    CHECK(xideEditorShow(app,xl)==XUI_OK && xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),0,xlOld)==XUI_OK);
    CHECK(xideAppFrame(app)==XGE_OK && !app->frameError && c->dirty && xl->dirty);
    f->cId=copy(c->sessionId);f->xlId=copy(xl->sessionId);CHECK(f->cId && f->xlId);
    if(!f->noSession) {
        CHECK(xideSessionSave(app,0,&error));
        f->metadata=xidePathResolve(xideSessionDirectory(app),"session.json");CHECK(f->metadata);
        if(f->previous) {
            wchar_t* native=xidePathNativeWide(f->metadata);
            f->lock=native ? CreateFileW(native,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE;
            free(native);CHECK(f->lock!=INVALID_HANDLE_VALUE);
        }
        if(f->protectedStorage)CHECK(write(f->metadata,damaged));
    }
    edited=GetTickCount64();
    CHECK(xuiCodeDocumentBeginEdit(xuiCodeEditGetDocument(c->editor))==XUI_OK);
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),(int)strlen(cOld),"// 最新未保存 C 中文 😀\n")==XUI_OK);
    CHECK(xuiCodeDocumentEndEdit(xuiCodeEditGetDocument(c->editor))==XUI_OK);
    CHECK(xuiCodeDocumentBeginEdit(xuiCodeEditGetDocument(xl->editor))==XUI_OK);
    CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),(int)strlen(xlOld),"// 最新未保存 xlang 中文 😀\n")==XUI_OK);
    CHECK(xuiCodeDocumentEndEdit(xuiCodeEditGetDocument(xl->editor))==XUI_OK);
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(c->editor),xuiCodeEditGetDocument(c->editor),4,0)==XUI_OK);
    CHECK(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(xl->editor),xuiCodeEditGetDocument(xl->editor),5,0)==XUI_OK);
    CHECK(!strcmp(xuiCodeEditGetText(c->editor),cNew) && !strcmp(xuiCodeEditGetText(xl->editor),xlNew));
    if(!f->boundary)app->proxy.surfaceClear=failedClear;
    f->inject=1;result=xideAppFrame(app);f->inject=0;
    CHECK(result==f->code && app->frameError==f->code && f->failedAt>=edited && f->failedAt-edited<750);
    printf("Frame failure domain observed: boundary=%d code=%d domain=%s recovery=%d\n",f->boundary,app->failure.code,app->failure.domain,app->failure.recovery);fflush(stdout);
    CHECK(app->failure.kind==XIDE_FAILURE_RENDER && app->failure.code==f->code && !strcmp(app->failure.domain,f->boundary ? "XGE" : "XUI"));
    CHECK(app->failure.recovery==(f->noSession ? XIDE_FAILURE_RECOVERY_DISABLED : f->previous || f->protectedStorage ? XIDE_FAILURE_RECOVERY_FAILED : XIDE_FAILURE_RECOVERY_SAVED));
    {
        char* diagnostics=xidePathResolve(f->directory,"diagnostics");xide_failure_report report={0};wchar_t message[40000];
        CHECK(diagnostics && xideFailureWrite(&app->failure,diagnostics,app->frames,&report));free(diagnostics);
        CHECK(xideFailureMessage(&app->failure,&report,message,40000));
        CHECK(wcsstr(message,f->noSession ? L"已关闭会话恢复" : f->previous || f->protectedStorage ? L"未能保存新的恢复检查点" : L"已保存可恢复的文档快照"));
    }
    CHECK(!strcmp(xuiCodeEditGetText(c->editor),cNew) && !strcmp(xuiCodeEditGetText(xl->editor),xlNew));
    CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(c->editor))==XUI_OK && !strcmp(xuiCodeEditGetText(c->editor),cOld));
    CHECK(xuiCodeDocumentRedo(xuiCodeEditGetDocument(c->editor))==XUI_OK && !strcmp(xuiCodeEditGetText(c->editor),cNew));
    CHECK(xuiCodeDocumentUndo(xuiCodeEditGetDocument(xl->editor))==XUI_OK && !strcmp(xuiCodeEditGetText(xl->editor),xlOld));
    CHECK(xuiCodeDocumentRedo(xuiCodeEditGetDocument(xl->editor))==XUI_OK && !strcmp(xuiCodeEditGetText(xl->editor),xlNew));
    if(f->protectedStorage)CHECK(matches(f->metadata,damaged));
    if(f->noSession)CHECK(!app->session && !xidePathExists(f->storage));
    printf("Saved fixture IDs %s %s\n",f->cId,f->xlId);
    printf("frame_failure_test failure passed: %d checks; code=%d beforeCheckpointDelayMs=%llu locked=%d protected=%d noSession=%d\n",f->checks,result,(unsigned long long)(f->failedAt-edited),f->previous,f->protectedStorage,f->noSession);fflush(stdout);
cleanup:
    if(f->lock!=INVALID_HANDLE_VALUE) { CloseHandle(f->lock);f->lock=INVALID_HANDLE_VALUE; }
    xideAppDestroy(app);xgeSetQuitRequestCallback(NULL,NULL);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;
    xge_desc_t d={0};d.iWidth=1280;d.iHeight=800;d.sTitle="xlang IDE private frame failure recovery verification";
    d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,f);
    xideAppDestroy(&f->app);xgeUnit();return result==XGE_OK && f->started && !f->failed?0:1;
}
int main(int argc,char** argv)
{
    if(argc<2 || argc>5)return 2;
    fixture f={0};f.directory=argv[1];f.lock=INVALID_HANDLE_VALUE;f.code=XUI_ERROR_GPU_FAILED;
    if(argc==5 && (!strcmp(argv[2],"--restore") || !strcmp(argv[2],"--restore-previous"))) {
        f.restore=1;f.previous=!strcmp(argv[2],"--restore-previous");f.cId=copy(argv[3]);f.xlId=copy(argv[4]);if(!f.cId || !f.xlId)return 1;
    } else if(argc==3) {
        if(!strcmp(argv[2],"--xge-begin")) { f.boundary=1;f.code=XGE_ERROR_BACKEND_FAILED; }
        else if(!strcmp(argv[2],"--xge-end")) { f.boundary=2;f.code=XGE_ERROR_INVALID_STATE; }
        else if(!strcmp(argv[2],"--oom"))f.code=XUI_ERROR_OUT_OF_MEMORY;
        else if(!strcmp(argv[2],"--locked"))f.previous=1;
        else if(!strcmp(argv[2],"--protected"))f.protectedStorage=1;
        else if(!strcmp(argv[2],"--no-session"))f.noSession=1;
        else return 2;
    } else if(argc!=2)return 2;
    wchar_t name[100];swprintf(name,100,L"XlangIDEFrameFailure-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);
    if(!f.desktop)return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;
    if(thread) {
        if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0) { fputs("frame_failure_test: native verification timed out\n",stderr);ExitProcess(1); }
        GetExitCodeThread(thread,&code);CloseHandle(thread);
    }
    CloseDesktop(f.desktop);free(f.settings);free(f.storage);free(f.cId);free(f.xlId);free(f.metadata);return (int)code;
}
