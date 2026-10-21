#include "app/xide_app.h"
#include "ui/configuration_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "theme/theme.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(test) do { if(!(test)) { fprintf(stderr,"configuration_test:%d: %s; %s\n",__LINE__,#test,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* value) { return (xstrview){value,strlen(value)}; }
static xvalue* get(const xvalue* root,const char* key) { return xrtValueObjectGet(root,sv(key)); }
static int clickAt(xide_app* app,int x,int y)
{
    return xuiInputPointerMove(app->context,(float)x,(float)y,0)==XUI_OK && xuiInputPointerDown(app->context,(float)x,(float)y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(app->context,(float)x,(float)y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && xuiDispatchPendingEvents(app->context)==XUI_OK;
}
static int click(xide_app* app,int control)
{
    xideWorkbenchLayout(app); if(xuiLayout(app->context)!=XUI_OK) return 0;
    xui_rect_t rect=xuiWidgetGetWorldRect(xideConfigurationViewControl(app,control)); return clickAt(app,rect.fX+rect.fW/2,rect.fY+rect.fH/2);
}
static int choose(xide_app* app,int control,int index)
{
    xui_widget combo=xideConfigurationViewControl(app,control); if(xuiComboBoxOpen(combo)!=XUI_OK || xuiLayout(app->context)!=XUI_OK) return 0;
    xui_widget menu=xuiComboBoxGetMenuWidget(combo); xui_rect_t item=xuiMenuGetItemRect(menu,index),rect=xuiWidgetGetWorldRect(menu);
    return clickAt(app,rect.fX+item.fX+item.fW/2,rect.fY+item.fY+item.fH/2) && xuiComboBoxGetSelected(combo)==index;
}
int main(int argc,char** argv)
{
    xide_app app={0}; xide_project loaded={0}; xide_model_error error={0}; xvalue *before=NULL,*drafts=NULL; int failed=0;
    char *root=NULL,*entry=NULL,*projectPath=NULL,*workspacePath=NULL,*copyPath=NULL;
    CHECK(argc==2); root=xidePathResolve(argv[1],"中文 配置"); CHECK(root && xrtDirCreateAll(root));
    entry=xidePathResolve(root,"main.xl"); projectPath=xidePathResolve(root,"app.xlp"); workspacePath=xidePathResolve(root,"sample.xlw"); copyPath=xidePathResolve(root,"copy/app.xlp");
    CHECK(entry && projectPath && workspacePath && copyPath);
    CHECK(xrtFileWriteAtomic(entry,(xbytesview){(unsigned char*)"func main(...) as int\n    return 0\nend\n",39}));
    CHECK(xideProjectCreate(&loaded,projectPath,"配置测试",entry,&error) && xideProjectSave(&loaded,NULL,&error));
    app.hidden=1; app.viewportWidth=1280; app.viewportHeight=800;
    CHECK(xuiCreate(&app.context)==XUI_OK && ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(app.context,1280,800)==XUI_OK && xideWorkbenchCreate(&app)==XUI_OK);
    CHECK(xideWorkspaceCreate(&app.workspaceModel,"配置工作空间",&error) && xideWorkspaceAddProject(&app.workspaceModel,projectPath,"alpha",&error));
    CHECK(xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,"alpha") && xideWorkspaceSetStartup(&app.workspaceModel,"alpha",&error));
    xide_project* project=xideWorkspaceFindProject(&app.workspaceModel,"alpha"); before=xideProjectConfigurationsJson(project); CHECK(before);
    CHECK(xideConfigurationViewOpen(&app)); xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_NAME),"取消的草稿");
    CHECK(click(&app,XIDE_CONFIG_CLONE) && click(&app,XIDE_CONFIG_CANCEL) && !xideConfigurationViewIsOpen(&app));
    drafts=xideProjectConfigurationsJson(project); CHECK(xrtValueEqual(before,drafts)); xrtValueRelease(drafts); drafts=NULL;
    CHECK(xideConfigurationViewOpen(&app) && click(&app,XIDE_CONFIG_CLONE));
    xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_NAME),"发布 中文配置");
    xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_OUTPUT),"build/发布/app.exe");
    xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_DIRECTORY),".");
    CHECK(choose(&app,XIDE_CONFIG_CATEGORY,3));
    xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_VALUE),""); CHECK(click(&app,XIDE_CONFIG_ADD));
    xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_VALUE),"含 空格与\"引号\"\\尾部\\"); CHECK(click(&app,XIDE_CONFIG_ADD));
    CHECK(click(&app,XIDE_CONFIG_UP));
    CHECK(choose(&app,XIDE_CONFIG_CATEGORY,4));
    xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_KEY),"XLANG_SAMPLE"); xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_VALUE),"中文=值"); CHECK(click(&app,XIDE_CONFIG_ADD));
    xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_KEY),"xlang_sample"); CHECK(click(&app,XIDE_CONFIG_ADD));
    CHECK(xuiListViewGetItemCount(xideConfigurationViewControl(&app,XIDE_CONFIG_LIST))==1);
    xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_KEY),"TEMP_ITEM"); CHECK(click(&app,XIDE_CONFIG_ADD) && click(&app,XIDE_CONFIG_REMOVE));
    CHECK(xuiListViewGetItemCount(xideConfigurationViewControl(&app,XIDE_CONFIG_LIST))==1);
    CHECK(choose(&app,XIDE_CONFIG_CATEGORY,0)); xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_VALUE),"include 空格"); CHECK(click(&app,XIDE_CONFIG_ADD));
    CHECK(xuiComboBoxGetItemCount(xideConfigurationViewControl(&app,XIDE_CONFIG_CATEGORY))==11);
    const char* cValues[]={"system 中文","generated/module.h","C_ANSWER=42","_CRT_SECURE_NO_WARNINGS"};
    for(int category=5;category<9;++category) {
        CHECK(choose(&app,XIDE_CONFIG_CATEGORY,category));
        xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_VALUE),cValues[category-5]); CHECK(click(&app,XIDE_CONFIG_ADD));
        CHECK(xuiListViewGetItemCount(xideConfigurationViewControl(&app,XIDE_CONFIG_LIST))==1);
    }
    xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_VALUE),"BAD=value"); CHECK(click(&app,XIDE_CONFIG_ADD));
    CHECK(xuiListViewGetItemCount(xideConfigurationViewControl(&app,XIDE_CONFIG_LIST))==1);
    CHECK(xideConfigurationViewApply(&app,&error) && project->configurationCount==2 && !xideConfigurationViewIsOpen(&app));
    CHECK(!project->configurations[0].arguments.count && !project->configurations[0].includePaths.count && !xrtValueCount(project->configurations[0].environment));
    CHECK(!strcmp(project->configurations[1].name,"发布 中文配置") && !strcmp(project->activeConfiguration,project->configurations[1].id));
    CHECK(project->configurations[1].cAnalysis.systemIncludes.count==1 && project->configurations[1].cAnalysis.forcedIncludes.count==1 && !strcmp(project->configurations[1].cAnalysis.defines.items[0],"C_ANSWER=42") && !strcmp(project->configurations[1].cAnalysis.undefines.items[0],"_CRT_SECURE_NO_WARNINGS"));
    CHECK(project->configurations[1].arguments.count==2 && !strcmp(project->configurations[1].arguments.items[0],"含 空格与\"引号\"\\尾部\\") && !*project->configurations[1].arguments.items[1]);
    xstrview environment; CHECK(xrtValueGetString(get(project->configurations[1].environment,"XLANG_SAMPLE"),&environment) && environment.Size==strlen("中文=值"));
    CHECK(xideWorkspaceSaveAll(&app,workspacePath,&error) && xideProjectLoad(&loaded,projectPath,&error) && !strcmp(loaded.activeConfiguration,loaded.configurations[1].id));
    CHECK(xideConfigurationViewOpen(&app) && click(&app,XIDE_CONFIG_NEW) && click(&app,XIDE_CONFIG_DELETE));
    CHECK(xuiComboBoxGetItemCount(xideConfigurationViewControl(&app,XIDE_CONFIG_PICKER))==2 && click(&app,XIDE_CONFIG_CANCEL));
    CHECK(xideConfigurationViewOpen(&app)); xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_NAME),"");
    CHECK(!xideConfigurationViewApply(&app,&error) && error.code==XIDE_MODEL_INVALID && !strcmp(project->configurations[1].name,"发布 中文配置") && click(&app,XIDE_CONFIG_CANCEL));
    CHECK(xideConfigurationViewOpen(&app) && choose(&app,XIDE_CONFIG_PICKER,0)); xuiEditSetText(xideConfigurationViewControl(&app,XIDE_CONFIG_NAME),"保留切换前的草稿");
    CHECK(choose(&app,XIDE_CONFIG_PICKER,1) && choose(&app,XIDE_CONFIG_PICKER,0) && !strcmp(xuiEditGetText(xideConfigurationViewControl(&app,XIDE_CONFIG_NAME)),"保留切换前的草稿"));
    CHECK(click(&app,XIDE_CONFIG_CANCEL));
    CHECK(xideEditorOpen(&app,projectPath,&error)); xui_code_document core=xuiCodeEditGetDocument(app.activeDocument->editor); CHECK(xuiCodeDocumentInsert(core,xuiCodeDocumentGetLength(core),"\n")==XUI_OK);
    CHECK(xideConfigurationViewOpen(&app) && !xideConfigurationViewApply(&app,&error) && error.code==XIDE_MODEL_CONFLICT && xideConfigurationViewIsOpen(&app));
    CHECK(click(&app,XIDE_CONFIG_CANCEL) && xuiCodeDocumentUndo(core)==XUI_OK);
    drafts=xideProjectConfigurationsJson(project); CHECK(drafts);
    CHECK(xrtValueObjectSetNew(xrtValueArrayGet(drafts,0),sv("future"),xrtValueString(sv("preserve"))) && xrtValueObjectSetNew(get(xrtValueArrayGet(drafts,0),"run"),sv("futureRun"),xrtValueInt(7)));
    CHECK(xideWorkspaceViewApplyConfigurations(&app,"alpha",drafts,project->activeConfiguration,&error)); xrtValueRelease(drafts); drafts=xideProjectConfigurationsJson(project); CHECK(drafts);
    CHECK(xrtValueArrayAppend(drafts,xrtValueArrayGet(drafts,0)) && !xideProjectApplyConfigurations(project,drafts,"development",&error) && error.code==XIDE_MODEL_CONFLICT && project->configurationCount==2);
    xrtValueRelease(drafts); drafts=xideProjectConfigurationsJson(project); CHECK(drafts && get(xrtValueArrayGet(drafts,0),"future") && get(get(xrtValueArrayGet(drafts,0),"run"),"futureRun"));
    CHECK(!xideProjectApplyConfigurations(project,drafts,"unknown",&error) && error.code==XIDE_MODEL_INVALID);
    CHECK(xideWorkspaceViewSelectConfiguration(&app,"development",&error) && xideWorkspaceSaveAll(&app,workspacePath,&error));
    char* copyDirectory=xidePathDirectory(copyPath); CHECK(copyDirectory && xrtDirCreateAll(copyDirectory)); free(copyDirectory);
    CHECK(xideProjectSave(&loaded,copyPath,&error) && xideProjectLoad(&loaded,copyPath,&error));
    CHECK(xidePathEqual(loaded.configurations[1].includePaths.items[0],project->configurations[1].includePaths.items[0]));
    CHECK(xidePathLexicalEqual(loaded.configurations[1].cAnalysis.systemIncludes.items[0],project->configurations[1].cAnalysis.systemIncludes.items[0]) && xidePathLexicalEqual(loaded.configurations[1].cAnalysis.forcedIncludes.items[0],project->configurations[1].cAnalysis.forcedIncludes.items[0]) && !strcmp(loaded.configurations[1].cAnalysis.defines.items[0],"C_ANSWER=42"));
    CHECK(xideWorkspaceAddProject(&app.workspaceModel,copyPath,"beta",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectProject(&app,"beta"));
    CHECK(!strcmp(xideWorkspaceFindProject(&app.workspaceModel,"alpha")->activeConfiguration,"development") && strcmp(xideWorkspaceFindProject(&app.workspaceModel,"beta")->activeConfiguration,"development"));
    CHECK(xideWorkspaceSaveAll(&app,workspacePath,&error));
    puts("configuration_test passed: native modal controls and menu selection, draft cancel/switch, clone, ordered empty/quoted arguments, environment values, per-project active configuration, manifest conflict, atomic validation, unknown fields and save-as rebasing.");
cleanup:
    xrtValueRelease(before); xrtValueRelease(drafts); xideAppDestroy(&app); xideProjectClear(&loaded); free(root); free(entry); free(projectPath); free(workspacePath); free(copyPath); return failed;
}