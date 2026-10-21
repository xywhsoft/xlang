#include "app/xide_app.h"
#include "ui/configuration_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "platform/path.h"
#include "services/ui_font.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char* root; HDESK desktop; int started,failed,checks; } fixture;
#define CHECK(v) do { ++f->checks; if(!(v)) { fprintf(stderr,"project_test_view_test:%d: %s; %s; %s\n",__LINE__,#v,error.message,info.error); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int settle(xide_app* a) { if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0; xideEditorPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a); return xuiLayout(a->context)==XUI_OK && !a->frameError; }
static int pointer(xide_app* a,xui_rect_t r) { float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static int click(xide_app* a,int id)
{
    xui_widget w=xideConfigurationViewControl(a,id),scroll=xideDialogGetScrollViewWidget(xideConfigurationViewWindow(a));
    if(xuiScrollViewEnsureChildVisible(scroll,w)!=XUI_OK || !settle(a)) return 0;
    return pointer(a,xuiWidgetGetWorldRect(w));
}
static int choose(xide_app* a,int index)
{
    xui_widget w=xideConfigurationViewControl(a,XIDE_CONFIG_CATEGORY);
    if(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideConfigurationViewWindow(a)),w)!=XUI_OK || !settle(a) || xuiComboBoxOpen(w)!=XUI_OK || !settle(a)) return 0;
    xui_widget menu=xuiComboBoxGetMenuWidget(w); xui_rect_t r=xuiMenuGetItemRect(menu,index),world=xuiWidgetGetWorldRect(menu); r.fX+=world.fX; r.fY+=world.fY;
    return pointer(a,r) && xuiComboBoxGetSelected(w)==index;
}
static int capture(xide_app* a,const char* p)
{ if(!settle(a) || xuiUpdate(a->context,0)!=XUI_OK) return 0; xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight}; return a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&r,1)==XUI_OK && xideAppCapture(a,p)==XGE_OK; }
static int resize(xide_app* a,int width,int height)
{ xui_surface_desc_t d={0}; d.iKind=XUI_SURFACE_KIND_TEXTURE; d.iWidth=width; d.iHeight=height; d.iFormat=XUI_SURFACE_FORMAT_RGBA8; d.iFlags=XUI_SURFACE_USAGE_TARGET|XUI_SURFACE_ALPHA_PREMULTIPLIED; xui_surface next=NULL; if(a->proxy.surfaceCreate(&a->proxy,&next,&d)!=XUI_OK) return 0; a->proxy.surfaceDestroy(&a->proxy,a->target); a->target=next; a->viewportWidth=width; a->viewportHeight=height; return xuiInputViewport(a->context,(float)width,(float)height)==XUI_OK && settle(a); }
static int waitTask(xide_app* a,xide_task* task,xide_task_info* info,int input)
{
    uint64_t end=GetTickCount64()+10000; int sent=0;
    do {
        if(!settle(a)) { return 0; } xideTaskInfo(task,info);
        if(input && !sent && xideTaskInputOpen(task)) {
            if(xuiTerminalInputText(a->panels[XIDE_TERMINAL],"原生测试输入😀\n")!=XUI_OK || xideActionExecute(a,XIDE_ACTION_CLOSE_INPUT)!=XIDE_ACTION_DISPATCHED) return 0;
            sent=1;
        }
        if(info->state>=XIDE_TASK_SUCCEEDED && xideTaskViewSettled(a)) return xuiTerminalFlush(a->panels[XIDE_TERMINAL])==XUI_OK;
        Sleep(5);
    } while(GetTickCount64()<end); return 0;
}
static int verify(void* data)
{
    fixture* f=data; if(f->started) return XGE_OK; f->started=1;
    xide_app app={0}; xide_model_error error={0}; xide_task_info info={0}; xide_project project={0}; xvalue *before=NULL,*after=NULL; int began=0;
    char *root=NULL,*entry=NULL,*native=NULL,*manifest=NULL,*beta=NULL,*program=NULL,*cwd=NULL,*settings=NULL,*fixturePath=NULL,*image=NULL,*outputImage=NULL,*largeImage=NULL;
    root=xidePathResolve(f->root,"native-project-test"); CHECK(root && xrtDirCreateAll(root));
    entry=xidePathResolve(root,"main.xl"); native=xidePathResolve(root,"main.c"); manifest=xidePathResolve(root,"project.xlp"); beta=xidePathResolve(root,"startup.xlp"); program=xidePathResolve(root,"测试 程序.exe"); cwd=xidePathResolve(root,"work 中文"); settings=xidePathResolve(root,"settings.json");
    image=xidePathResolve(root,"native-project-test-config.png"); outputImage=xidePathResolve(root,"native-project-test-terminal.png");
    largeImage=xidePathResolve(root,"native-project-test-large-font.png");
    wchar_t binary[32768]; CHECK(GetModuleFileNameW(NULL,binary,32768)); int n=WideCharToMultiByte(CP_UTF8,0,binary,-1,NULL,0,NULL,NULL); char* executable=malloc((size_t)n); CHECK(executable); WideCharToMultiByte(CP_UTF8,0,binary,-1,executable,n,NULL,NULL); char* dir=xidePathDirectory(executable); free(executable); fixturePath=dir ? xidePathResolve(dir,"xide_task_program.exe") : NULL; free(dir);
    CHECK(entry && native && manifest && beta && program && cwd && settings && image && outputImage && fixturePath && xrtDirCreateAll(cwd) && xrtFileCopy(fixturePath,program,true));
    CHECK(writeText(entry,"func main(...) as int\n return 0\nend\n") && writeText(native,"int native_answer(void) { return 42; }\n"));
    CHECK(xideProjectCreate(&project,manifest,"显式项目测试",entry,&error) && xideProjectAddFile(&project,native,"c",&error) && xideProjectSave(&project,NULL,&error)); xideProjectClear(&project);
    CHECK(xideProjectCreate(&project,beta,"独立启动项目",entry,&error) && xideProjectSave(&project,NULL,&error));
    app.hidden=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
    CHECK(xideWorkspaceCreate(&app.workspaceModel,"项目测试工作空间",&error) && xideWorkspaceAddProject(&app.workspaceModel,manifest,"alpha",&error) && xideWorkspaceAddProject(&app.workspaceModel,beta,"beta",&error) && xideWorkspaceSetStartup(&app.workspaceModel,"beta",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,"alpha"));
    CHECK(!xideTasksCount(app.tasks) && !xideActionQuery(&app,XIDE_ACTION_TEST).enabled && xideActionNamed("task.test")->id==78 && xideActionCount()==73);
    CHECK(xideEditorOpen(&app,entry,&error)); xide_document* xl=app.activeDocument; CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(xl->editor),xuiCodeDocumentGetLength(xuiCodeEditGetDocument(xl->editor)),"// 未保存 xl\n")==XUI_OK);
    CHECK(xideEditorOpen(&app,native,&error)); xide_document* c=app.activeDocument; CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(c->editor),xuiCodeDocumentGetLength(xuiCodeEditGetDocument(c->editor)),"// 未保存 C\n")==XUI_OK && settle(&app) && c->dirty && xl->dirty);
    xide_project* selected=xideWorkspaceFindProject(&app.workspaceModel,"alpha"); before=xideProjectConfigurationsJson(selected); CHECK(before);
    CHECK(xideConfigurationViewOpen(&app) && choose(&app,9));
    CHECK(xuiWidgetGetVisible(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_PROGRAM)) && !xuiWidgetGetVisible(xideConfigurationViewControl(&app,XIDE_CONFIG_OUTPUT)));
    CHECK(xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_PROGRAM),"不会执行.exe")==XUI_OK && click(&app,XIDE_CONFIG_CANCEL));
    after=xideProjectConfigurationsJson(selected); CHECK(xrtValueEqual(before,after) && !xideTasksCount(app.tasks)); xrtValueRelease(after); after=NULL;
    CHECK(xideActionExecute(&app,XIDE_ACTION_CONFIGURATIONS)==XIDE_ACTION_DISPATCHED && choose(&app,9));
    CHECK(xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_PROGRAM),"测试 程序.exe")==XUI_OK && xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_DIRECTORY),"work 中文")==XUI_OK);
    CHECK(xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_VALUE),"中文 参数")==XUI_OK && click(&app,XIDE_CONFIG_ADD));
    CHECK(xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_VALUE),"")==XUI_OK && click(&app,XIDE_CONFIG_ADD) && click(&app,XIDE_CONFIG_UP));
    CHECK(choose(&app,10) && xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_KEY),"XIDE_TASK_MODE")==XUI_OK && xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_VALUE),"echo")==XUI_OK && click(&app,XIDE_CONFIG_ADD));
    CHECK(xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_KEY),"xide_task_mode")==XUI_OK && click(&app,XIDE_CONFIG_ADD) && xuiListViewGetItemCount(xideConfigurationViewControl(&app,XIDE_CONFIG_LIST))==1);
    CHECK(choose(&app,0) && !xuiWidgetGetVisible(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_PROGRAM)) && choose(&app,9) && !strcmp(xuiEditGetText(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_PROGRAM)),"测试 程序.exe") && capture(&app,image));
    CHECK(click(&app,XIDE_CONFIG_APPLY) && !xideConfigurationViewIsOpen(&app) && !xideTasksCount(app.tasks) && c==app.activeDocument && c->dirty && xl->dirty && !strcmp(c->language,"c") && !strcmp(xl->language,"xlang"));
    CHECK(xideActionQuery(&app,XIDE_ACTION_TEST).enabled && xuiInputKeyDown(app.context,XUI_KEY_F7,XUI_MOD_CTRL)==XUI_OK && xuiInputKeyUp(app.context,XUI_KEY_F7,0)==XUI_OK && settle(&app));
    xide_task* task=xideTaskViewSelected(&app); CHECK(task && !strcmp(xideTaskProjectId(task),"alpha") && !strcmp(app.workspaceModel.startupProject,"beta") && !strcmp(app.selectedProject,"alpha") && !c->dirty && !xl->dirty);
    CHECK(waitTask(&app,task,&info,1) && info.state==XIDE_TASK_SUCCEEDED && info.kind==XIDE_TASK_KIND_TEST && info.tested && !info.built && !info.ran);
    char terminal[32768]={0}; CHECK(xuiTerminalSerializeText(app.panels[XIDE_TERMINAL],terminal,sizeof(terminal))>=0 && strstr(terminal,"ARG[]") && strstr(terminal,"中文 参数") && strstr(terminal,"原生测试输入😀"));
    CHECK(strstr(xuiTextEditGetText(app.panels[XIDE_OUTPUT]),"Raw test log") && xuiTreeViewGetNodeCount(app.panels[XIDE_TASKS])==1 && capture(&app,outputImage));
    CHECK(xideWorkspaceViewSelectProject(&app,"beta") && !xideActionQuery(&app,XIDE_ACTION_TEST).enabled && xideTasksCount(app.tasks)==1 && xideWorkspaceViewSelectProject(&app,"alpha"));
    CHECK(xideConfigurationViewOpen(&app) && choose(&app,9) && xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_PROGRAM),"")==XUI_OK && click(&app,XIDE_CONFIG_APPLY) && !xideActionQuery(&app,XIDE_ACTION_TEST).enabled && xideTasksCount(app.tasks)==1);
    CHECK(largeImage && xideConfigurationViewOpen(&app) && choose(&app,9) && xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_PROGRAM),"测试 程序.exe")==XUI_OK && xideUiFontApply(&app,app.uiFontPath,40,&error) && resize(&app,700,460));
    CHECK(xuiInputGetFont(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_PROGRAM))==app.uiFont && xuiWidgetGetRect(xideConfigurationViewControl(&app,XIDE_CONFIG_TEST_PROGRAM)).fH==xideUiHeight(&app,32) && xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideConfigurationViewWindow(&app)),xideConfigurationViewControl(&app,XIDE_CONFIG_APPLY))==XUI_OK && capture(&app,largeImage) && click(&app,XIDE_CONFIG_APPLY) && xideActionQuery(&app,XIDE_ACTION_TEST).enabled && xideTasksCount(app.tasks)==1);
    printf("project_test_view_test passed: %d checks; native configuration categories, cancelled draft no writes/execution, ordered/empty arguments and environment, independent startup, Ctrl+F7, save C/xlang before test, terminal input/close, test history/output, disable test and 3 native screenshots at 125%% DPI and 40 DIP popup scrolling.\n",f->checks);
cleanup:
    if(began) { xgeEnd(); } xideAppDestroy(&app); xideProjectClear(&project); xrtValueRelease(before); xrtValueRelease(after);
    free(root); free(entry); free(native); free(manifest); free(beta); free(program); free(cwd); free(settings); free(fixturePath); free(image); free(outputImage); free(largeImage); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{ fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang project test verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1])) { return 2; } fixture f={.root=argv[1]}; wchar_t name[100]; swprintf(name,100,L"XlangProjectTest-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0) ExitProcess(1); GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); return (int)code;
}
