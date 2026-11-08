#include "app/xide_app.h"
#include "ui/task_view.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture { HDESK desktop; const char* directory; int started,failed,checks; } fixture;
#define CHECK(e) do { ++f->checks; if(!(e)){fprintf(stderr,"build_output_wrap_test:%d: %s\n",__LINE__,#e);f->failed=1;goto cleanup;} } while(0)
static const char* sample="[Task 2] main.xl / single-file\nCompiler: D:/GIT/x-lang/release/win_x64/xl.exe\n[Task failed, 0 ms] save the input and request a new task\n中文输出与 emoji 😀\n";
static int settle(xide_app* app)
{
    xideWorkbenchLayout(app);
    return xuiDispatchPendingEvents(app->context)==XUI_OK && xuiUpdate(app->context,.016f)==XUI_OK && xuiLayout(app->context)==XUI_OK;
}
static int verify(void* user)
{
    fixture* f=user;if(f->started)return XGE_OK;f->started=1;
    xide_app app={0};int began=0;char* settings=xidePathResolve(f->directory,"settings.json");char* image=xidePathResolve(f->directory,"build-output.png");
    app.hidden=1;app.noSession=1;app.disableAutomaticDiagnostics=1;app.settingsPath=settings;
    CHECK(settings && image && xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
    xui_widget output=app.panels[XIDE_OUTPUT];CHECK(output && xuiCodeEditGetWordWrap(output) && xuiCodeEditIsReadonly(output));
    xui_dock_window_info_t info={.iSize=sizeof(info)};
    CHECK(xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_OUTPUT],&info)==XUI_OK);
    CHECK(xuiDockPanelSetPaneActiveWindow(app.dock,info.iPane,app.windows[XIDE_OUTPUT])==XUI_OK);
    CHECK(xuiCodeEditSetText(output,sample)==XUI_OK && settle(&app));
    xui_code_document document=xuiCodeEditGetDocument(output);int lines=xuiCodeDocumentGetLineCount(document);
    CHECK(lines==5);
    for(int line=0;line<4;++line){
        int start,end;xui_rect_t a,b;
        CHECK(xuiCodeDocumentGetLineRange(document,line,&start,&end)==XUI_OK);
        CHECK(xuiCodeEditGetTextOffsetRect(output,start,&a)==XUI_OK && xuiCodeEditGetTextOffsetRect(output,end,&b)==XUI_OK);
        CHECK(a.fY==b.fY);
    }
    xui_rect_i_t rect={0,0,app.viewportWidth,app.viewportHeight};
    CHECK(app.proxy.surfaceClear(&app.proxy,app.target,0)==XUI_OK && xuiRender(app.context,app.target,&rect,1)==XUI_OK && xideAppCapture(&app,image)==XGE_OK);
    CHECK(xuiDockPanelFloatWindow(app.dock,app.windows[XIDE_OUTPUT],(xui_rect_t){300,100,360,300})==XUI_OK && settle(&app));
    int end=0;xui_rect_t a,b;CHECK(xuiCodeDocumentGetLineRange(document,2,NULL,&end)==XUI_OK);
    CHECK(xuiCodeEditGetTextOffsetRect(output,0,&a)==XUI_OK && xuiCodeEditGetTextOffsetRect(output,end,&b)==XUI_OK && b.fY>a.fY);
    CHECK(xuiRender(app.context,app.target,&rect,1)==XUI_OK && !strcmp(xuiCodeEditGetText(output),sample));
    printf("build_output_wrap_test passed: %d checks; actual IDE output widget, short lines, narrow wrapping, Unicode, unchanged document and native rendering.\n",f->checks);
cleanup:
    if(began)xgeEnd();
    xideAppDestroy(&app);free(settings);free(image);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;
    xge_desc_t desc={0};desc.iWidth=1600;desc.iHeight=900;desc.sTitle="Build output wrap verification";desc.iFlags=XGE_INIT_WINDOW;desc.iRunMode=XGE_RUN_GAME_LOOP;
    int status=xgeInit(&desc);if(status==XGE_OK)status=xgeRun(verify,f);xgeUnit();return status==XGE_OK && f->started && !f->failed ? 0:1;
}
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1]))return 2;
    fixture f={.directory=argv[1]};
    wchar_t name[80];swprintf(name,80,L"XlangOutputWrap-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;
    if(thread){if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}
    CloseDesktop(f.desktop);return (int)code;
}
