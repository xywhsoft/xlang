#include "app/xide_app.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "theme/theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(expression) do { if(!(expression)) { fprintf(stderr,"workspace_view_test:%d: %s; %s\n",__LINE__,#expression,error.message); failed=1; goto cleanup; } } while(0)
static int writeText(const char* path,const char* text) { return xrtFileWriteAtomic(path,(xbytesview){(unsigned char*)text,strlen(text)}); }
int main(int argc,char** argv)
{
    xide_app app={0}; xide_workspace saved={0}; xide_project project={0}; xide_model_error error={0}; int failed=0;
    char *root=NULL,*a=NULL,*b=NULL,*manifest=NULL,*scene=NULL,*entry=NULL,*native=NULL;
    CHECK(argc==2); root=xidePathResolve(argv[1],"中文 UI 工作空间"); CHECK(root && xrtDirCreateAll(root));
    a=xidePathResolve(root,"alpha.xlp"); b=xidePathResolve(root,"beta.xlp"); manifest=xidePathResolve(root,"sample.xlw"); scene=xidePathResolve(root,"scene.xlw"); entry=xidePathResolve(root,"main.xl"); native=xidePathResolve(root,"helper.c");
    CHECK(a && b && manifest && scene && entry && native);
    CHECK(writeText(entry,"func main(...) as int\n    print(\"真实项目源文件\")\n    return 0\nend\n") && writeText(native,"int native_helper(void) { return 42; }\n"));
    CHECK(xideProjectCreate(&project,a,"启动项目 Alpha",entry,&error) && xideProjectAddFile(&project,native,NULL,&error) && xideProjectSave(&project,a,&error));
    CHECK(xideProjectCreate(&project,b,"属性项目 Beta",entry,&error) && xideProjectSave(&project,b,&error));
    CHECK(xideWorkspaceCreate(&saved,"双语法工作空间",&error) && xideWorkspaceAddProject(&saved,a,"alpha",&error) && xideWorkspaceAddProject(&saved,b,"beta",&error));
    CHECK(xideWorkspaceSetStartup(&saved,"alpha",&error) && xideWorkspaceSave(&saved,scene,&error) && xideWorkspaceSave(&saved,manifest,&error));
    app.hidden=1; app.viewportWidth=1280; app.viewportHeight=800;
    CHECK(xuiCreate(&app.context)==XUI_OK && ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(app.context,1280,800)==XUI_OK);
    CHECK(xideWorkbenchCreate(&app)==XUI_OK);
    CHECK(xideWorkspaceOpen(&app,manifest,0,&error));
    CHECK(xuiTreeViewGetNodeCount(app.workspace)==9 && !strcmp(app.workspaceModel.startupProject,"alpha"));
    CHECK(xideWorkspaceViewSelectProject(&app,"beta") && !strcmp(app.selectedProject,"beta") && !strcmp(app.workspaceModel.startupProject,"alpha"));
    int selected=xuiTreeViewGetSelected(app.workspace);
    CHECK(xideWorkspaceViewApplyProperties(&app,"修改后的 Beta","描述","dll","main.xl",&error));
    CHECK(!strcmp(xideWorkspaceFindProject(&app.workspaceModel,"beta")->name,"修改后的 Beta") && !strcmp(app.workspaceModel.startupProject,"alpha"));
    CHECK(xuiTreeViewGetSelected(app.workspace)==selected && strstr(xuiTreeViewGetNodeById(app.workspace,selected)->sText,"修改后的 Beta"));
    CHECK(!xideWorkspaceViewApplyProperties(&app,"不应应用","无效","exe","wrong.c",&error));
    CHECK(!strcmp(xideWorkspaceFindProject(&app.workspaceModel,"beta")->name,"修改后的 Beta"));
    CHECK(xideWorkspaceSaveAll(&app,manifest,&error));
    CHECK(xideWorkspaceLoad(&saved,manifest,&error) && !strcmp(saved.startupProject,"alpha") && !strcmp(xideWorkspaceFindProject(&saved,"beta")->kind,"dll"));
    CHECK(xideEditorOpen(&app,b,&error));
    xide_document* manifestDocument=app.activeDocument; xui_code_document manifestCore=xuiCodeEditGetDocument(manifestDocument->editor);
    CHECK(xuiCodeDocumentInsert(manifestCore,xuiCodeDocumentGetLength(manifestCore),"\n ")==XUI_OK && manifestDocument->dirty);
    CHECK(!xideWorkspaceViewApplyProperties(&app,"Blocked","描述","dll","main.xl",&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(!xideWorkspaceSaveAll(&app,manifest,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(xuiCodeDocumentUndo(manifestCore)==XUI_OK && !manifestDocument->dirty);
    CHECK(xideWorkspaceViewApplyProperties(&app,"修改后的 Beta","已验证冲突保护","dll","main.xl",&error));
    CHECK(xuiCodeDocumentInsert(manifestCore,xuiCodeDocumentGetLength(manifestCore),"\n ")==XUI_OK);
    CHECK(!xideDocumentSave(manifestDocument,NULL,0,&error) && error.code==XIDE_MODEL_CONFLICT && manifestDocument->dirty);
    CHECK(xuiCodeDocumentUndo(manifestCore)==XUI_OK && !manifestDocument->dirty && xideWorkspaceSaveAll(&app,manifest,&error));
    CHECK(strstr(xuiCodeEditGetText(manifestDocument->editor),"已验证冲突保护") && !xideDocumentCheckExternal(manifestDocument));
    xideDispatchAction(&app,XIDE_ACTION_SET_STARTUP);
    CHECK(!strcmp(app.workspaceModel.startupProject,"beta"));
    CHECK(xideWorkspaceViewSelectProject(&app,"alpha") && !strcmp(app.workspaceModel.startupProject,"beta"));
    xide_project* alpha=xideWorkspaceFindProject(&app.workspaceModel,"alpha");
    CHECK(xideWorkspaceViewSelectFile(&app,"alpha",alpha->files.items[0].id));
    xideWorkbenchLayout(&app); CHECK(xuiLayout(app.context)==XUI_OK);
    CHECK(xuiSetFocusWidget(app.context,app.workspace)==XUI_OK && xuiInputKeyDownEx(app.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && xuiInputKeyUpEx(app.context,XUI_KEY_ENTER,0,NULL)==XUI_OK && xuiDispatchPendingEvents(app.context)==XUI_OK);
    CHECK(app.activeDocument && strstr(xuiCodeEditGetText(app.activeDocument->editor),"真实项目源文件") && !strcmp(xuiCodeEditGetLanguage(app.activeDocument->editor),"xlang"));
    CHECK(xideWorkspaceViewSelectFile(&app,"alpha",alpha->files.items[1].id));
    int index=xuiTreeViewGetVisibleIndexOfId(app.workspace,xuiTreeViewGetSelected(app.workspace));
    xui_rect_t row=xuiTreeViewGetItemRect(app.workspace,index),treeRect=xuiWidgetGetWorldRect(app.workspace);
    float x=treeRect.fX+row.fX+row.fW-20,y=treeRect.fY+row.fY+row.fH/2;
    for(int i=0;i<2;++i) {
        CHECK(xuiInputPointerMove(app.context,x,y,0)==XUI_OK && xuiInputPointerDown(app.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(app.context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK);
    }
    CHECK(xuiDispatchPendingEvents(app.context)==XUI_OK);
    CHECK(xidePathEqual(app.activeDocument->path,native) && !strcmp(xuiCodeEditGetLanguage(app.activeDocument->editor),"c") && strstr(xuiCodeEditGetText(app.activeDocument->editor),"native_helper"));
    CHECK(xideProjectAddFile(alpha,"lost.txt",NULL,&error));
    CHECK(xideWorkspaceViewRefresh(&app)==XUI_OK);
    CHECK(xideWorkspaceViewSelectFile(&app,"alpha",alpha->files.items[alpha->files.count-1].id));
    CHECK(strstr(xuiTreeViewGetNodeById(app.workspace,xuiTreeViewGetSelected(app.workspace))->sText,"缺失"));
    xideDispatchAction(&app,XIDE_ACTION_REMOVE_FILE);
    CHECK(alpha->files.count==2 && xidePathExists(entry) && xidePathExists(native));
    CHECK(xideWorkspaceViewSelectProject(&app,"beta"));
    xideDispatchAction(&app,XIDE_ACTION_REMOVE_PROJECT);
    CHECK(app.workspaceModel.projectCount==1 && !*app.workspaceModel.startupProject && xidePathExists(b));
    CHECK(xideWorkspaceSaveAll(&app,manifest,&error));
    puts("workspace_view_test passed: real three-layer tree, independent selection/startup, validated property edits, persistence, editable C/xlang via Enter/double-click, manifest/property conflict protection, missing-file maintenance and reference-only removal");
cleanup:
    xideAppDestroy(&app); xideWorkspaceClear(&saved); xideProjectClear(&project);
    free(root); free(a); free(b); free(manifest); free(scene); free(entry); free(native); return failed;
}