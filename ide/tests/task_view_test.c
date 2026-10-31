#include "app/xide_app.h"
#include "ui/task_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "platform/settings.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(test) do { if(!(test)) { fprintf(stderr,"task_view_test:%d: %s; model=%s; task=%s\n",__LINE__,#test,error.message,info.error); failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,strlen(text)}); }
static int waitTask(xide_app* app,xide_task* task,xide_task_info* info,unsigned timeout,int sendInput)
{
    ULONGLONG began=GetTickCount64(); int sent=0;
    do {
        xideTaskViewPoll(app); xuiDispatchPendingEvents(app->context); xideTaskInfo(task,info);
        if(info->phase==XIDE_TASK_RUN && sendInput && !sent && xideTaskWrite(task,"",0)) {
            if(xuiTerminalInputText(app->panels[XIDE_TERMINAL],"终端输入\n")!=XUI_OK) return 0;
            xideDispatchAction(app,XIDE_ACTION_CLOSE_INPUT); sent=1;
        }
        if(info->state>=XIDE_TASK_SUCCEEDED) { xideTaskViewPoll(app); xuiTerminalFlush(app->panels[XIDE_TERMINAL]); return 1; }
        Sleep(5);
    } while(GetTickCount64()-began<timeout); return 0;
}
int main(int argc,char** argv)
{
    int failed=0; xide_model_error error={0}; xide_task_info info={0}; xide_app app={0}; xide_workspace workspace={0};
    char *root=NULL,*alpha=NULL,*beta=NULL,*entry=NULL,*workspacePath=NULL,*binaryDir=NULL,*compiler=NULL,*preferences=NULL,*resource=NULL,*external=NULL,*unrelated=NULL;
    xide_settings preference={0}; xide_project configured={0}; xvalue* configurations=NULL; wchar_t* previousCompiler=NULL; int environmentChanged=0;
    CHECK(argc==2); root=xidePathResolve(argv[1],"中文 任务界面"); CHECK(root && xrtDirCreateAll(root));
    alpha=xidePathResolve(root,"alpha.xlp"); beta=xidePathResolve(root,"beta.xlp"); entry=xidePathResolve(root,"main.xl"); workspacePath=xidePathResolve(root,"scene.xlw"); CHECK(alpha && beta && entry && workspacePath);
    const char* alphaJson="{\"format\":\"xlang-project-1\",\"kind\":\"exe\",\"entry\":\"main.xl\",\"ide\":{\"format\":\"xlang-ide-project-1\",\"name\":\"Alpha 运行项目\",\"configurations\":[{\"id\":\"dev\",\"name\":\"开发\",\"output\":\"build/alpha.exe\",\"run\":{\"workingDirectory\":\".\",\"arguments\":[\"中文 参数\"],\"environment\":{\"XIDE_TASK_MODE\":\"echo\"}}}]}}";
    const char* betaJson="{\"format\":\"xlang-project-1\",\"kind\":\"dll\",\"entry\":\"main.xl\",\"ide\":{\"format\":\"xlang-ide-project-1\",\"name\":\"Beta 构建项目\",\"configurations\":[{\"id\":\"dev\",\"name\":\"开发\",\"output\":\"build/beta.dll\"}]}}";
    CHECK(writeText(alpha,alphaJson) && writeText(beta,betaJson) && writeText(entry,"FAKE_OK\n"));
    CHECK(xideProjectLoad(&configured,alpha,&error)); configurations=xideProjectConfigurationsJson(&configured); CHECK(configurations);
    xvalue* runtime=xrtValueDeepClone(xrtValueArrayGet(configurations,0)); CHECK(runtime);
    CHECK(xrtValueObjectSetNew(runtime,(xstrview){"id",2},xrtValueString((xstrview){"runtime",7})) && xrtValueObjectSetNew(runtime,(xstrview){"name",4},xrtValueString((xstrview){"运行专用",strlen("运行专用")})) && xrtValueObjectSetNew(runtime,(xstrview){"output",6},xrtValueString((xstrview){"build/runtime.exe",17})));
    CHECK(xrtValueArrayAppendNew(configurations,runtime) && xideProjectApplyConfigurations(&configured,configurations,"runtime",&error) && xideProjectSave(&configured,NULL,&error));
    xrtValueRelease(configurations); configurations=NULL; xideProjectClear(&configured);
    CHECK(xideWorkspaceCreate(&workspace,"任务界面工作空间",&error) && xideWorkspaceAddProject(&workspace,alpha,"alpha",&error) && xideWorkspaceAddProject(&workspace,beta,"beta",&error) && xideWorkspaceSetStartup(&workspace,"alpha",&error) && xideWorkspaceSave(&workspace,workspacePath,&error));
    binaryDir=xidePathDirectory(argv[0]); compiler=binaryDir ? xidePathResolve(binaryDir,"task_test.exe") : NULL; CHECK(compiler); preferences=xidePathResolve(root,"preferences.json"); CHECK(preferences && writeText(preferences,"{\"format\":\"xlang-ide-settings-1\"}") && xideSettingsLoad(&preference,preferences,&error) && xideSettingsSetCompiler(&preference,compiler,&error));
    DWORD environmentSize=GetEnvironmentVariableW(L"XLANG_COMPILER",NULL,0); if(environmentSize) { previousCompiler=malloc((size_t)environmentSize*sizeof(*previousCompiler)); CHECK(previousCompiler && GetEnvironmentVariableW(L"XLANG_COMPILER",previousCompiler,environmentSize)); }
    CHECK(SetEnvironmentVariableW(L"XLANG_COMPILER",NULL)); environmentChanged=1; app.settingsPath=preferences; app.hidden=1;
    app.viewportWidth=1280; app.viewportHeight=800; CHECK(xuiCreate(&app.context)==XUI_OK && ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(app.context,1280,800)==XUI_OK && xideWorkbenchCreate(&app)==XUI_OK);
    CHECK(xideTaskViewCompiler(&app) && xidePathEqual(xideTaskViewCompiler(&app),compiler));
    CHECK(SetEnvironmentVariableW(L"XLANG_COMPILER",previousCompiler)); environmentChanged=0;
    CHECK(xideWorkspaceOpen(&app,workspacePath,0,&error) && xideWorkspaceViewSelectProject(&app,"beta")); xideTaskViewPoll(&app);
    for(int i=XIDE_HELP;i<=XIDE_TASKS;++i) CHECK(xuiDockPanelHideWindow(app.dock,app.windows[i])==XUI_OK);
    xideDispatchAction(&app,XIDE_ACTION_BUILD); xide_task* task=xideTaskViewSelected(&app); CHECK(task && !strcmp(xideTaskProjectId(task),"beta") && !strcmp(xideTaskConfigurationId(task),"dev"));
    CHECK(waitTask(&app,task,&info,10000,0) && info.state==XIDE_TASK_SUCCEEDED && info.built && !info.ran);
    xui_dock_window_info_t dock={0}; dock.iSize=sizeof(dock); CHECK(xuiDockPanelGetWindowInfo(app.dock,app.windows[XIDE_OUTPUT],&dock)==XUI_OK && dock.iRegion==XUI_DOCK_PANEL_REGION_DOCUMENT && dock.iState==XUI_DOCK_PANEL_WINDOW_DOCKED);
    CHECK(strstr(xuiCodeEditGetText(app.panels[XIDE_OUTPUT]),"Build succeeded") && xuiTreeViewGetNodeCount(app.panels[XIDE_TASKS])==1);
    CHECK(xideEditorOpen(&app,entry,&error)); CHECK(xuiCodeEditSetText(app.activeDocument->editor,"FAKE_OK\n// 未保存的输入\n")==XUI_OK); CHECK(xuiDispatchPendingEvents(app.context)==XUI_OK && app.activeDocument->dirty);
    xideDispatchAction(&app,XIDE_ACTION_RUN); task=xideTaskViewSelected(&app); CHECK(task && !strcmp(xideTaskProjectId(task),"alpha") && !strcmp(xideTaskConfigurationId(task),"runtime") && !app.activeDocument->dirty && !strcmp(app.selectedProject,"beta"));
    CHECK(waitTask(&app,task,&info,10000,1) && info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran);
    char terminal[32768]={0}; CHECK(xuiTerminalSerializeText(app.panels[XIDE_TERMINAL],terminal,sizeof(terminal))>=0); CHECK(strstr(terminal,"终端输入") && strstr(terminal,"中文 参数"));
    CHECK(xuiCodeEditSetText(app.activeDocument->editor,"FAKE_FAIL\n\n中文😀abc\n")==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK);
    xideDispatchAction(&app,XIDE_ACTION_RUN); task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info,10000,0) && info.state==XIDE_TASK_FAILED && !info.ran);
    CHECK(xuiTreeViewGetNodeCount(app.panels[XIDE_PROBLEMS])==1 && strstr(xuiCodeEditGetText(app.panels[XIDE_OUTPUT]),"真实失败"));
    CHECK(xideTaskViewJump(&app,0)); int start=0,end=0,lineStart=0,lineEnd=0;
    CHECK(xuiCodeSelectionGetRange(xuiCodeEditGetSelection(app.activeDocument->editor),&start,&end)==XUI_OK && xuiCodeDocumentGetLineRange(xuiCodeEditGetDocument(app.activeDocument->editor),2,&lineStart,&lineEnd)==XUI_OK);
    CHECK(start==lineStart+(int)strlen("中文😀a") && end==start);
    CHECK(xideTaskViewSelect(&app,xideTasksAt(app.tasks,0))==XUI_OK); xideTaskViewPoll(&app); CHECK(strstr(xuiCodeEditGetText(app.panels[XIDE_OUTPUT]),"Beta 构建项目") && xuiTreeViewGetNodeCount(app.panels[XIDE_PROBLEMS])==0);
    CHECK(xuiCodeEditSetText(app.activeDocument->editor,"FAKE_OK\n")==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK);
    xide_project* running=xideWorkspaceFindProject(&app.workspaceModel,"alpha"); CHECK(running);
    CHECK(xrtValueObjectSetNew(xideProjectFindConfiguration(running,running->activeConfiguration)->environment,(xstrview){"XIDE_TASK_MODE",14},xrtValueString((xstrview){"flood",5}))); running->dirty=1;
    xideDispatchAction(&app,XIDE_ACTION_RUN); task=xideTaskViewSelected(&app); CHECK(task && waitTask(&app,task,&info,10000,0) && info.state==XIDE_TASK_SUCCEEDED);
    for(int i=0;i<30;++i) xideTaskViewPoll(&app);
    const char* visible=xuiCodeEditGetText(app.panels[XIDE_OUTPUT]); CHECK(strlen(visible)>128u*1024u && strstr(visible,"显示已省略") && strstr(visible,"\xef\xbf\xbd"));
    CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,visible,-1,NULL,0)>0);
    resource=xidePathResolve(root,"app.res.json"); external=xidePathResolve(argv[1],"外置 清单.xml"); unrelated=xidePathResolve(argv[1],"无关 C 文档.c");
    CHECK(resource && external && unrelated && writeText(external,"original manifest") && writeText(unrelated,"int unrelated = 1;\n"));
    xvalue* res=xrtValueObject(); CHECK(res && xrtValueObjectSetNew(res,(xstrview){"manifest",8},xrtValueString((xstrview){external,strlen(external)})));
    size_t resourceSize=0; char* resourceBody=xrtJsonStringify(res,false,&resourceSize); xrtValueRelease(res); CHECK(resourceBody);
    int written=writeText(resource,resourceBody); xrtFree(resourceBody); CHECK(written && xideProjectAddFile(running,resource,"resource-config",&error));
    running->dirty=1; CHECK(xideEditorOpen(&app,external,&error)); xide_document* assetDocument=app.activeDocument;
    CHECK(xuiCodeEditSetText(assetDocument->editor,"unsaved outside project manifest")==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK && assetDocument->dirty);
    CHECK(xideEditorOpen(&app,unrelated,&error)); xide_document* cDocument=app.activeDocument;
    CHECK(xuiCodeEditSetText(cDocument->editor,"int unrelated = 2;\n")==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK && cDocument->dirty && !strcmp(cDocument->language,"c"));
    xideDispatchAction(&app,XIDE_ACTION_RUN); task=xideTaskViewSelected(&app);
    CHECK(task && waitTask(&app,task,&info,10000,0) && info.state==XIDE_TASK_FAILED && info.invalidated && !info.built && !info.ran);
    CHECK(xideTaskDiagnosticCount(task)==1);
    xide_diagnostic pendingNotice={0}; CHECK(xideTaskDiagnostic(task,0,&pendingNotice));
    int pendingMatches=pendingNotice.kind==XIDE_DIAGNOSTIC_UNSAVED_INPUT && !pendingNotice.line && !pendingNotice.column && xidePathEqual(pendingNotice.path,external);
    xideDiagnosticClear(&pendingNotice); CHECK(pendingMatches);
    CHECK(assetDocument->dirty && cDocument->dirty && app.activeDocument==cDocument && !strcmp(app.selectedProject,"beta") && !strcmp(app.workspaceModel.startupProject,"alpha"));
    CHECK(xideTaskViewJump(&app,0) && app.activeDocument==assetDocument && assetDocument->dirty && cDocument->dirty);
    CHECK(!strcmp(app.selectedProject,"beta") && !strcmp(app.workspaceModel.startupProject,"alpha"));
    CHECK(xideDocumentSave(assetDocument,NULL,0,&error) && !assetDocument->dirty && cDocument->dirty);
    xideDispatchAction(&app,XIDE_ACTION_RUN); task=xideTaskViewSelected(&app);
    CHECK(task && waitTask(&app,task,&info,10000,0) && info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && cDocument->dirty);
    puts("task_view_test resource input gate passed: external dirty manifest blocks old build/run, explicit save unblocks, unrelated C document remains dirty and selected/startup targets are unchanged.");
    for(int i=0;i<IDE_THEME_COUNT;++i) CHECK(xideWorkbenchApplyTheme(&app,i)==XUI_OK);
    CHECK(xideWorkbenchReset(&app)==XUI_OK && xuiLayout(app.context)==XUI_OK);
    CHECK(xideDocumentSave(cDocument,NULL,0,&error));
    CHECK(xideWorkspaceOpen(&app,alpha,1,&error) && app.workspaceModel.projectCount==1 && !strcmp(app.workspaceModel.startupProject,app.workspaceModel.projects[0].id));
    puts("task_view_test passed: native build/run actions, selected versus startup project, save before build, restored center-only bottom, task history, terminal input Unicode diagnostic jump, bounded binary/Unicode output standalone startup, restored compiler preferences and independent active build/run configurations.");
cleanup:
    if(environmentChanged) SetEnvironmentVariableW(L"XLANG_COMPILER",previousCompiler);
    free(previousCompiler); free(preferences); xideSettingsClear(&preference); xideProjectClear(&configured); xrtValueRelease(configurations);
    xideAppDestroy(&app); xideWorkspaceClear(&workspace); free(root); free(alpha); free(beta); free(entry); free(workspacePath); free(binaryDir); free(compiler); free(resource); free(external); free(unrelated); return failed;
}
