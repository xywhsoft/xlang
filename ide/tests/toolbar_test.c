#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "services/execution_target.h"
#include "services/ui_font.h"
#include "ui/main_toolbar.h"
#include "ui/task_view.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "ui/reading_state.h"
#include "ui/configuration_view.h"
#include "theme/theme.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler,*program;HDESK desktop;HANDLE gate;int checks,failed,started; } fixture;
#define CHECK(e) do { ++f->checks;if(!(e)) { fprintf(stderr,"toolbar_test:%d: %s; %s\n",__LINE__,#e,error.message);f->failed=1;goto cleanup; } } while(0)
static int frame(xide_app* a) { return xideAppFrame(a)==XGE_OK && !a->frameError; }
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int click(xide_app* a,xui_widget w)
{
    xui_rect_t r=xuiWidgetGetWorldRect(w);float x=r.fX+r.fW/2,y=r.fY+r.fH/2;
    return r.fW>0 && r.fH>0 && xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && frame(a);
}
static int choose(xide_app* a,int control,const char* prefix)
{
    if(!click(a,xideMainToolbarControl(a,control)))return 0;
    xui_widget menu=xideMainToolbarMenu(a);if(!xuiMenuIsOpen(menu))return 0;
    for(int i=0;i<xuiMenuGetItemCount(menu);++i) {
        const xui_menu_item_t* item=xuiMenuGetItem(menu,i);
        if(item->sText && !strncmp(item->sText,prefix,strlen(prefix))) {
            xui_rect_t r=xuiMenuGetItemRect(menu,i),world=xuiWidgetGetWorldRect(menu);float x=world.fX+r.fX+r.fW/2,y=world.fY+r.fY+r.fH/2;
            return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && frame(a);
        }
    }
    return 0;
}
static int geometry(xide_app* a)
{
    xui_rect_t host=xuiWidgetGetWorldRect(a->toolbar);
    for(int i=0;i<XIDE_TB_COUNT;++i) {
        xui_widget w=xideMainToolbarControl(a,i);if(!xuiWidgetGetVisible(w))continue;xui_rect_t r=xuiWidgetGetWorldRect(w);
        if(r.fW<=0 || r.fH<=0 || r.fX<host.fX-0.1f || r.fY<host.fY-0.1f || r.fX+r.fW>host.fX+host.fW+0.1f || r.fY+r.fH>host.fY+host.fH+0.1f) { fprintf(stderr,"toolbar bounds control=%d width=%d rect=%d/%d\n",i,host.fW,r.fX,r.fW);return 0; }
        for(int j=i+1;j<XIDE_TB_COUNT;++j)if(xuiWidgetGetVisible(xideMainToolbarControl(a,j))) {
            xui_rect_t other=xuiWidgetGetWorldRect(xideMainToolbarControl(a,j));if(r.fX<other.fX+other.fW-0.1f && other.fX<r.fX+r.fW-0.1f) { fprintf(stderr,"toolbar overlap %d/%d\n",i,j);return 0; }
        }
    }
    return xuiWidgetGetVisible(xideMainToolbarControl(a,XIDE_TB_RUN)) && xuiWidgetGetVisible(xideMainToolbarControl(a,XIDE_TB_STOP));
}
static int waitState(xide_app* a,xide_task* task,int completed)
{
    ULONGLONG until=GetTickCount64()+15000;
    do { if(!frame(a))return 0;xide_task_info info={0};xideTaskInfo(task,&info);if(completed ? info.state>=XIDE_TASK_SUCCEEDED : info.state==XIDE_TASK_EXECUTING && strstr(xuiCodeEditGetText(a->panels[XIDE_OUTPUT]),"BUILD_GATE_WAITING"))return 1;Sleep(1); } while(GetTickCount64()<until);
    return 0;
}
static int capture(xide_app* a,const char* root,const char* name)
{
    char* path=xidePathResolve(root,name);int ok=path && xideAppCapture(a,path)==XGE_OK;free(path);return ok;
}
static int run(void* user)
{
    fixture* f=user;f->started=1;xide_app app={0};xide_model_error error={0};xide_workspace model={0};xvalue* state=NULL;
    char *entry=NULL,*alpha=NULL,*beta=NULL,*ws=NULL,*settings=NULL;char json[8192];
    CHECK(xrtDirCreateAll(f->directory));entry=xidePathResolve(f->directory,"main.xl");alpha=xidePathResolve(f->directory,"alpha.xlp");beta=xidePathResolve(f->directory,"beta.xlp");ws=xidePathResolve(f->directory,"toolbar.xlw");settings=xidePathResolve(f->directory,"settings.json");CHECK(entry && alpha && beta && ws && settings);
    CHECK(writeText(entry,"FAKE_GATE\n"));
    snprintf(json,sizeof(json),"{\"format\":\"xlang-project-1\",\"kind\":\"exe\",\"entry\":\"main.xl\",\"ide\":{\"name\":\"Alpha\",\"configurations\":[{\"id\":\"debug\",\"name\":\"A 开发\",\"output\":\"build/a.exe\"},{\"id\":\"release\",\"name\":\"A 发布\",\"output\":\"build/ar.exe\",\"test\":{\"program\":\"%s\",\"environment\":{\"XIDE_TASK_MODE\":\"fail\"}}}]}}",f->program);CHECK(writeText(alpha,json));
    CHECK(writeText(beta,"{\"format\":\"xlang-project-1\",\"kind\":\"dll\",\"entry\":\"main.xl\",\"ide\":{\"name\":\"Beta\",\"configurations\":[{\"id\":\"debug\",\"name\":\"B 开发\",\"output\":\"build/b.dll\"},{\"id\":\"release\",\"name\":\"B 发布\",\"output\":\"build/br.dll\"}]}}"));
    CHECK(xideWorkspaceCreate(&model,"工具栏验收",&error) && xideWorkspaceAddProject(&model,alpha,"A",&error) && xideWorkspaceAddProject(&model,beta,"B",&error) && xideWorkspaceSetStartup(&model,"A",&error) && xideWorkspaceSave(&model,ws,&error));
    app.hidden=1;app.noSession=1;app.disableAutomaticDiagnostics=1;app.maxFrames=10000;app.settingsPath=settings;app.compilerPath=f->compiler;app.dpiOverride=1;
    CHECK(xideAppCreate(&app)==XUI_OK && frame(&app) && geometry(&app));
    CHECK(!xideActionQuery(&app,XIDE_ACTION_BUILD).enabled && !xideActionQuery(&app,XIDE_ACTION_STOP).enabled);
    CHECK(capture(&app,f->directory,"toolbar-empty.png"));
    CHECK(xideWorkspaceOpen(&app,ws,0,&error) && xideWorkspaceViewSelectProject(&app,"B") && frame(&app));
    xide_execution_context context;xideExecutionResolve(&app,0,&context);
    CHECK(context.project && !strcmp(context.project->id,"A") && xideActionQuery(&app,XIDE_ACTION_BUILD).enabled && xideActionQuery(&app,XIDE_ACTION_RUN).enabled);
    app.configurationId="missing";CHECK(frame(&app) && !xideActionQuery(&app,XIDE_ACTION_BUILD).enabled);
    CHECK(choose(&app,XIDE_TB_CONFIGURATION,"A 发布"));xideExecutionResolve(&app,0,&context);
    CHECK(context.configuration && !strcmp(context.configuration->id,"release") && !context.commandLineConfiguration && !strcmp(app.selectedProject,"B") && !app.workspaceModel.projects[0].dirty);
    app.configurationId=NULL;
    CHECK(xideActionQuery(&app,XIDE_ACTION_TEST).enabled && choose(&app,XIDE_TB_TARGET,"项目：Beta"));
    CHECK(!xideActionQuery(&app,XIDE_ACTION_RUN).enabled && strstr(xideActionQuery(&app,XIDE_ACTION_RUN).reason,"库"));
    CHECK(choose(&app,XIDE_TB_CONFIGURATION,"B 发布") && click(&app,xideMainToolbarControl(&app,XIDE_TB_BUILD)));
    xide_task* b=xideTaskViewSelected(&app);CHECK(b && !strcmp(xideTaskProjectId(b),"B") && !strcmp(xideTaskConfigurationId(b),"release") && waitState(&app,b,0));
    CHECK(choose(&app,XIDE_TB_TARGET,"项目：Alpha") && !strcmp(app.selectedProject,"B"));
    CHECK(xuiInputKeyDownEx(app.context,XUI_KEY_F7,0,NULL)==XUI_OK && xuiInputKeyUpEx(app.context,XUI_KEY_F7,0,NULL)==XUI_OK && frame(&app));
    xide_task* a=xideTaskViewSelected(&app);CHECK(a && a!=b && !strcmp(xideTaskProjectId(a),"A") && !strcmp(xideTaskConfigurationId(a),"release"));
    CHECK(click(&app,xideMainToolbarControl(&app,XIDE_TB_STOP)) && waitState(&app,a,1));
    xide_task_info ai={0},bi={0};xideTaskInfo(a,&ai);xideTaskInfo(b,&bi);CHECK(ai.state==XIDE_TASK_CANCELLED && bi.state==XIDE_TASK_EXECUTING && !xideActionQuery(&app,XIDE_ACTION_STOP).enabled && xideActionQuery(&app,XIDE_ACTION_STOP_ALL).enabled);
    CHECK(choose(&app,XIDE_TB_STOP_MENU,"Beta ·") && xideTaskViewSelected(&app)==b && xideActionQuery(&app,XIDE_ACTION_STOP).enabled);
    CHECK(capture(&app,f->directory,"toolbar-active.png"));
    CHECK(xideActionExecute(&app,XIDE_ACTION_STOP_ALL)==XIDE_ACTION_DISPATCHED && waitState(&app,b,1));xideTaskInfo(b,&bi);CHECK(bi.state==XIDE_TASK_CANCELLED);
    CHECK(click(&app,xideMainToolbarControl(&app,XIDE_TB_TEST)) && waitState(&app,xideTaskViewSelected(&app),1));
    xide_task_info failed={0};xideTaskInfo(xideTaskViewSelected(&app),&failed);CHECK(failed.state==XIDE_TASK_FAILED && failed.kind==XIDE_TASK_KIND_TEST);
    CHECK(choose(&app,XIDE_TB_CONFIGURATION,"编辑目标配置") && xideConfigurationViewIsOpen(&app));
    CHECK(strstr(xuiComboBoxGetText(xideConfigurationViewControl(&app,XIDE_CONFIG_PICKER)),"A "));
    CHECK(click(&app,xideConfigurationViewControl(&app,XIDE_CONFIG_CANCEL)) && !xideConfigurationViewIsOpen(&app));
    CHECK(xideEditorOpen(&app,entry,&error) && choose(&app,XIDE_TB_TARGET,"当前文件"));xideExecutionResolve(&app,0,&context);CHECK(context.file && context.document==app.activeDocument && !xideActionQuery(&app,XIDE_ACTION_TEST).enabled);
    xide_document* document=app.activeDocument;uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(document->editor));
    CHECK(choose(&app,XIDE_TB_TARGET,"项目：Alpha"));state=xideReadingCapture(&app,NULL);CHECK(state && xideReadingValid(state));
    xideExecutionReset(&app);CHECK(xideReadingRestore(&app,state,0));xideExecutionResolve(&app,0,&context);CHECK(!context.file && !strcmp(context.projectId,"A") && !strcmp(context.configurationId,"release"));
    CHECK(xideSettingsRememberOpen(&app.preferences,entry,&error) && xideSettingsRememberOpen(&app.preferences,ws,&error) && xideSettingsRememberOpen(&app.preferences,entry,&error));
    const xvalue* recent=xrtValueObjectGet(app.preferences.source,(xstrview){"recent",6});CHECK(xrtValueCount(recent)==2);
    for(int theme=0;theme<IDE_THEME_COUNT;++theme)for(int dpi=0;dpi<3;++dpi) {
        app.dpiOverride=dpi==0?1:dpi==1?1.25f:2;CHECK(xideWorkbenchApplyTheme(&app,theme)==XUI_OK && frame(&app) && frame(&app) && geometry(&app));
        CHECK(document->open && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(document->editor))==version);
        if(theme==IDE_THEME_GRAPHITE) { char name[64];snprintf(name,sizeof(name),"toolbar-dpi-%d.png",dpi);CHECK(capture(&app,f->directory,name)); }
    }
    app.dpiOverride=1;CHECK(frame(&app) && xideUiFontApply(&app,app.uiFontPath,24,&error) && frame(&app) && geometry(&app));
    CHECK(capture(&app,f->directory,"toolbar-font-24.png"));
    CHECK(xideUiFontApply(&app,app.uiFontPath,16,&error) && frame(&app));
    app.dpiOverride=2;CHECK(frame(&app) && !xuiWidgetGetVisible(xideMainToolbarControl(&app,XIDE_TB_TARGET)));
    CHECK(choose(&app,XIDE_TB_CONTEXT,"项目：Beta"));xideExecutionResolve(&app,0,&context);CHECK(context.project && !strcmp(context.project->id,"B"));
    CHECK(choose(&app,XIDE_TB_CONTEXT,"项目：Alpha") && choose(&app,XIDE_TB_MORE,"查看任务与输出"));
    app.dpiOverride=1;CHECK(frame(&app));
    const float widths[]={1280,1000,800,640,480,360};for(int i=0;i<6;++i) { app.viewportWidth=(int)widths[i];CHECK(xuiInputViewport(app.context,widths[i],900)==XUI_OK);xideWorkbenchLayout(&app);CHECK(xuiLayout(app.context)==XUI_OK && geometry(&app)); }
    CHECK(xideExecutionSelect(&app,XIDE_TARGET_PROJECT,"B") && xideWorkspaceRemoveProject(&app.workspaceModel,"B",&error));xideExecutionResolve(&app,0,&context);CHECK(context.reason && !xideActionQuery(&app,XIDE_ACTION_BUILD).enabled);
    xvalue* invalid=xrtValueBool(true);CHECK(invalid);int rejected=!xideExecutionRestore(&app,invalid);xrtValueRelease(invalid);CHECK(rejected);
    printf("toolbar_test passed: %d checks; native target/config menus, same-target keyboard/build/run, CLI override, library guard, actual queued/current/all stop, failed test, target settings, per-workspace state, recent paths, four themes, DPI/font and six widths\n",f->checks);fflush(stdout);
cleanup:
    SetEvent(f->gate);xrtValueRelease(state);xideAppDestroy(&app);xideWorkspaceClear(&model);free(entry);free(alpha);free(beta);free(ws);free(settings);xgeSetQuitRequestCallback(NULL,NULL);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="Toolbar native verification";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,f);xgeUnit();return result==XGE_OK && f->started && !f->failed?0:1;
}
int main(int argc,char** argv)
{
    if(argc!=2)return 2;
    char* bin=xidePathDirectory(argv[0]);char *compiler=bin ? xidePathResolve(bin,"task_test.exe") : NULL,*program=bin ? xidePathResolve(bin,"xide_task_program.exe") : NULL;free(bin);if(!compiler || !program)return 3;
    fixture f={.directory=argv[1],.compiler=compiler,.program=program};wchar_t name[100],gate[100];swprintf(name,100,L"Toolbar-%lu",GetCurrentProcessId());swprintf(gate,100,L"Local\\ToolbarBuild-%lu",GetCurrentProcessId());
    f.gate=CreateEventW(NULL,TRUE,FALSE,gate);if(!f.gate || !SetEnvironmentVariableW(L"XIDE_TEST_BUILD_GATE",gate))return 4;
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 5;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0)ExitProcess(6);GetExitCodeThread(thread,&code);CloseHandle(thread); }
    CloseDesktop(f.desktop);CloseHandle(f.gate);free(compiler);free(program);return (int)code;
}
