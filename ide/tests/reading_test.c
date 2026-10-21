#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/reading_state.h"
#include "ui/catalog_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "services/session.h"
#include "services/ui_font.h"
#include "platform/state_file.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"reading_test:%d: %s; %s\n",__LINE__,#e,error.message); f->failed=1; goto cleanup; } } while(0)
typedef struct fixture { const char* directory; HDESK desktop; int started,failed; } fixture;
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,sv(k)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) ? s.Data : ""; }
static int writeText(const char* path,const char* s) { return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static int start(xide_app* app,const char* storage,const char* manifest,const char* settings,const char* catalog)
{
    memset(app,0,sizeof(*app)); app->hidden=1; app->highDpi=1; app->dpiOverride=1.25f; app->maxFrames=10000;
    app->sessionRoot=storage; app->workspacePath=manifest; app->settingsPath=settings; app->catalogPath=catalog; return xideAppCreate(app)==XUI_OK;
}
static int settle(xide_app* app)
{
    if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;
    xideEditorPoll(app); xideWorkbenchLayout(app); if(xuiLayout(app->context)!=XUI_OK) return 0;
    xideCatalogViewLayout(app); if(xuiLayout(app->context)!=XUI_OK) return 0;
    xideCatalogViewAfterLayout(app); if(xuiLayout(app->context)!=XUI_OK) return 0;
    xideSessionAfterLayout(app); return xuiLayout(app->context)==XUI_OK && !app->frameError;
}
static int find(xide_app* app,xui_widget tree,const char* wanted,int workspace)
{
    for(int i=0;i<xuiTreeViewGetNodeCount(tree);++i) { const xui_tree_view_node_t* n=xuiTreeViewGetNode(tree,i); char* key=workspace ? xideWorkspaceTreeKey(app,n->iId) : xideCatalogTreeKey(app,n->iId); int same=key && !strcmp(key,wanted); xrtFree(key); if(same) return n->iId; }
    return -1;
}
static int sameAnchor(const xvalue* before,const xvalue* after,const char* tree)
{
    const xvalue *a=get(before,tree),*b=get(after,tree);
    return !strcmp(text(get(a,"selected")),text(get(b,"selected"))) && !strcmp(text(get(a,"anchor")),text(get(b,"anchor"))) &&
        fabs(xideReadingNumber(get(a,"fraction"),0)-xideReadingNumber(get(b,"fraction"),0))<0.002;
}
static int sameHelp(const xvalue* before,const xvalue* after)
{
    const xvalue *a=get(get(before,"help"),"position"),*b=get(get(after,"help"),"position");
    return !strcmp(text(get(get(a,"block"),"id")),text(get(get(b,"block"),"id"))) &&
        fabs(xideReadingNumber(get(get(a,"block"),"fraction"),0)-xideReadingNumber(get(get(b,"block"),"fraction"),0))<0.02 &&
        xrtValueEqual(get(a,"chapter"),get(b,"chapter"));
}
static int capture(xide_app* app,const char* path)
{
    xui_rect_i_t r={0,0,app->viewportWidth,app->viewportHeight};
    return xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&r,1)==XUI_OK && xideAppCapture(app,path)==XUI_OK;
}
static int verify(void* user)
{
    fixture* f=user; f->started=1; int began=0; xide_app app={0}; xide_workspace workspace={0}; xide_project project={0}; xide_state_file state={0}; xide_model_error error={0};
    char *root=NULL,*storage=NULL,*settings=NULL,*entry=NULL,*native=NULL,*projectPath=NULL,*otherProject=NULL,*manifest=NULL,*other=NULL,*note=NULL,*metadata=NULL,*firstImage=NULL,*secondImage=NULL,*fileKey=NULL,*moduleKey=NULL,*font=NULL,*catalogPath=NULL;
    xvalue *before=NULL,*after=NULL,*invalid=NULL; unsigned char* raw=NULL;
    char* uuid=xideCreateId("reading"); root=uuid ? xidePathResolve(f->directory,uuid) : NULL; free(uuid); CHECK(root && xrtDirCreateAll(root));
    storage=xidePathResolve(root,"中文 阅读会话"); settings=xidePathResolve(root,"settings.json"); entry=xidePathResolve(root,"main.xl"); native=xidePathResolve(root,"native.c"); projectPath=xidePathResolve(root,"alpha.xlp"); otherProject=xidePathResolve(root,"beta.xlp"); manifest=xidePathResolve(root,"first.xlw"); other=xidePathResolve(root,"second.xlw"); note=xidePathResolve(root,"说明.txt"); firstImage=xidePathResolve(root,"native-reading-first.png"); secondImage=xidePathResolve(root,"native-reading-restored.png");
    CHECK(storage && settings && entry && native && projectPath && otherProject && manifest && other && note && firstImage && secondImage);
    CHECK(writeText(entry,"func main(...) as int\n    return 0\nend\n") && writeText(native,"/* 中文 C 文档 */\nint helper(void) { return 3; }\n") && writeText(note,"工作空间级说明\n"));
    CHECK(xideProjectCreate(&project,projectPath,"阅读项目 Alpha",entry,&error));
    for(int i=0;i<80;++i) { char path[100]; snprintf(path,sizeof(path),"src/文件-%d.c",i); CHECK(xideProjectAddFile(&project,path,"c",&error)); }
    CHECK(xideProjectSave(&project,NULL,&error)); fileKey=xideReadingKey("file","alpha",project.files.items[75].id); CHECK(fileKey);
    CHECK(xideProjectCreate(&project,otherProject,"阅读项目 Beta",entry,&error) && xideProjectSave(&project,NULL,&error));
    CHECK(xideWorkspaceCreate(&workspace,"独立阅读状态",&error) && xideWorkspaceAddProject(&workspace,projectPath,"alpha",&error) && xideWorkspaceAddProject(&workspace,otherProject,"beta",&error) && xideWorkspaceAddFile(&workspace,note,&error) && xideWorkspaceSetStartup(&workspace,"alpha",&error) && xideWorkspaceSave(&workspace,manifest,&error) && xideWorkspaceSave(&workspace,other,&error));
    CHECK(start(&app,storage,manifest,settings,NULL) && xgeBegin()==XGE_OK); began=1; CHECK(settle(&app));
    CHECK(xideEditorOpen(&app,native,&error) && xideEditorOpen(&app,entry,&error) && settle(&app));
    char* beta=xideReadingKey("project","beta",""); CHECK(beta); int betaId=find(&app,app.workspace,beta,1); xrtFree(beta); CHECK(betaId>=0 && xuiTreeViewSetNodeExpanded(app.workspace,betaId,0)==XUI_OK);
    int originalFile=find(&app,app.workspace,fileKey,1); CHECK(originalFile>=0 && xideWorkspaceTreeSelect(&app,originalFile) && !strcmp(app.selectedProject,"alpha"));
    const xide_command* split=xideCatalogFind(&app.catalog,"standard:string.split"); if(!split) for(size_t i=0;i<app.catalog.count;++i) if(!strcmp(app.catalog.entries[i].name,"string.split")) { split=&app.catalog.entries[i]; break; }
    const xide_command* func=xideCatalogFind(&app.catalog,"language:func"); CHECK(split && func); moduleKey=xideReadingKey("module","standard",split->module); CHECK(moduleKey);
    CHECK(xuiTreeViewSetNodeExpanded(app.commands,2,1)==XUI_OK && xuiTreeViewSetNodeExpanded(app.commands,3,1)==XUI_OK);
    int module=find(&app,app.commands,moduleKey,0); CHECK(module>=0 && xuiTreeViewSetNodeExpanded(app.commands,module,1)==XUI_OK);
    xideWorkbenchShowHelp(&app,(int)(func-app.catalog.entries)); CHECK(xuiTreeViewSetNodeExpanded(app.commands,1,0)==XUI_OK && xuiTreeViewSetSelected(app.commands,module)==XUI_OK);
    CHECK(xideHelpJump(xideCatalogHelpDocument(&app),3) && settle(&app));
    float hx=0,hy=0; CHECK(xuiScrollViewGetOffset(app.help,&hx,&hy)==XUI_OK && hy>100 && xuiScrollViewSetOffset(app.help,hx,hy+30)==XUI_OK);
    CHECK(xuiTreeViewSetScroll(app.workspace,500.5f)==XUI_OK && xuiTreeViewSetScroll(app.commands,150.5f)==XUI_OK && xuiScrollViewSetOffset(app.properties,0,70)==XUI_OK && settle(&app));
    before=xideReadingCapture(&app,NULL); CHECK(before && xideReadingValid(before) && get(get(get(before,"help"),"position"),"block"));
    CHECK(xideSessionSave(&app,0,&error) && capture(&app,firstImage)); metadata=xidePathResolve(xideSessionDirectory(&app),"session.json"); CHECK(metadata && xideStateOpen(&state,metadata,"xlang-ide-session-1",&error));
    invalid=xrtValueDeepClone(state.source); CHECK(invalid && xrtValueObjectSetNew(get(invalid,"reading"),sv("futureReading"),xrtValueInt(17)) && xrtValueObjectSetNew(xrtValueArrayGet(get(get(get(invalid,"reading"),"commands"),"nodes"),0),sv("futureNode"),xrtValueInt(23)) && xideStateWrite(&state,invalid,&error)); xrtValueRelease(invalid); invalid=NULL; xideStateClear(&state);
    CHECK(xideProjectLoad(&project,projectPath,&error)); xide_file swapped=project.files.items[1]; project.files.items[1]=project.files.items[75]; project.files.items[75]=swapped; CHECK(xideProjectSave(&project,NULL,&error));
    // Reorder real catalog entries, changing SDK command/module IDs on restart.
    catalogPath=xidePathResolve(root,"commands.json"); after=xrtValueDeepClone(app.catalog.source); CHECK(catalogPath && after);
    xvalue* entries=get(after,"entries"),*reordered=xrtValueArray(); CHECK(entries && reordered);
    size_t splitIndex=SIZE_MAX; for(size_t i=0;i<xrtValueCount(entries);++i) if(!strcmp(text(get(xrtValueArrayGet(entries,i),"id")),split->id)) { splitIndex=i; break; }
    CHECK(splitIndex!=SIZE_MAX && xrtValueArrayAppend(reordered,xrtValueArrayGet(entries,splitIndex)));
    for(size_t i=0;i<xrtValueCount(entries);++i) if(i!=splitIndex) CHECK(xrtValueArrayAppend(reordered,xrtValueArrayGet(entries,i)));
    CHECK(xrtValueObjectSetNew(after,sv("entries"),reordered)); size_t catalogBytes=0; char* catalogJson=xrtJsonStringify(after,false,&catalogBytes);
    CHECK(catalogJson && writeText(catalogPath,catalogJson)); xrtFree(catalogJson); xrtValueRelease(after); after=NULL;
    font=xideUiFontDefaultPath(); CHECK(font && xideUiFontApply(&app,font,24,&error));
    after=xideReadingCapture(&app,NULL); CHECK(after && sameAnchor(before,after,"workspace") && sameAnchor(before,after,"commands") && sameHelp(before,after)); xrtValueRelease(after); after=NULL;
    xideAppDestroy(&app); CHECK(start(&app,storage,manifest,settings,catalogPath) && settle(&app) && app.uiFontSize==24);
    int restoredFile=find(&app,app.workspace,fileKey,1); CHECK(restoredFile>=0 && restoredFile!=originalFile && xuiTreeViewGetSelected(app.workspace)==restoredFile && !strcmp(app.selectedProject,"alpha") && !strcmp(app.workspaceModel.startupProject,"alpha"));
    CHECK(xuiTreeViewGetNodeExpanded(app.commands,1)==0 && xuiTreeViewGetNodeExpanded(app.commands,2)==1 && xuiTreeViewGetSelected(app.commands)==find(&app,app.commands,moduleKey,0) && find(&app,app.commands,moduleKey,0)!=module);
    after=xideReadingCapture(&app,NULL); CHECK(after && sameAnchor(before,after,"workspace") && sameAnchor(before,after,"commands") && sameHelp(before,after)); xrtValueRelease(after); after=NULL;
    CHECK(app.documents.count>=2 && app.activeDocument && !strcmp(app.activeDocument->language,"xlang"));
    uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(app.activeDocument->editor));
    app.viewportWidth=900; app.viewportHeight=600; CHECK(xuiInputViewport(app.context,900,600)==XUI_OK && settle(&app) && xideReadingRestore(&app,before,1) && settle(&app));
    after=xideReadingCapture(&app,NULL); CHECK(after && sameHelp(before,after) && version==xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(app.activeDocument->editor))); xrtValueRelease(after); after=NULL;
    app.viewportWidth=1280; app.viewportHeight=800; CHECK(xuiInputViewport(app.context,1280,800)==XUI_OK && settle(&app) && xideReadingRestore(&app,before,1) && settle(&app) && capture(&app,secondImage));
    // The end of the document follows reflow rather than an obsolete offset.
    CHECK(xuiScrollViewSetOffset(app.help,0,10000000)==XUI_OK && settle(&app)); after=xideReadingCapture(&app,NULL); bool atEnd=false;
    CHECK(after && xrtValueGetBool(get(get(get(after,"help"),"position"),"end"),&atEnd) && atEnd);
    CHECK(xuiScrollViewSetOffset(app.help,0,0)==XUI_OK && xideReadingRestore(&app,after,1) && settle(&app));
    xvalue* end=xideReadingCapture(&app,NULL); CHECK(end && xrtValueGetBool(get(get(get(end,"help"),"position"),"end"),&atEnd) && atEnd); xrtValueRelease(end); xrtValueRelease(after); after=NULL;
    CHECK(xideReadingRestore(&app,before,1) && settle(&app));
    // Explicit startup chapter overrides the saved reading position.
    app.initialChapterRequested=1; CHECK(xideHelpJump(xideCatalogHelpDocument(&app),1) && settle(&app));
    float overrideY=0; CHECK(xuiScrollViewGetOffset(app.help,NULL,&overrideY)==XUI_OK && xideReadingRestore(&app,before,1));
    float keptY=0; CHECK(xuiScrollViewGetOffset(app.help,NULL,&keptY)==XUI_OK && overrideY==keptY);
    app.initialChapterRequested=0; CHECK(xideReadingRestore(&app,before,1) && settle(&app));
    // Missing identities are ignored while live selections and source stay intact.
    invalid=xrtValueDeepClone(before); CHECK(invalid && xrtValueObjectSetNew(get(invalid,"workspace"),sv("selected"),xrtValueString(sv("removed-node"))) && xideReadingValid(invalid));
    int currentSelection=xuiTreeViewGetSelected(app.workspace); CHECK(xideReadingRestore(&app,invalid,0) && xuiTreeViewGetSelected(app.workspace)==currentSelection);
    xrtValueRelease(invalid); invalid=NULL;
    // Workspace-level file selection preserves the previous valid property owner.
    CHECK(xideWorkspaceViewSelectFile(&app,"",app.workspaceModel.files.items[0].id) && !strcmp(app.selectedProject,"alpha"));
    CHECK(xideReadingRestore(&app,before,0) && settle(&app) && xideReadingRestore(&app,before,1));
    CHECK(xideSessionSave(&app,0,&error) && xideStateOpen(&state,metadata,"xlang-ide-session-1",&error));
    CHECK(get(get(state.source,"reading"),"futureReading") && get(xrtValueArrayGet(get(get(get(state.source,"reading"),"commands"),"nodes"),0),"futureNode")); xideStateClear(&state);
    CHECK(xideWorkspaceOpen(&app,other,0,&error) && settle(&app) && xuiTreeViewGetNodeExpanded(app.commands,2)==0 && xuiTreeViewGetScroll(app.workspace)==0);
    CHECK(xideWorkspaceOpen(&app,manifest,0,&error) && settle(&app)); after=xideReadingCapture(&app,NULL); CHECK(after && sameHelp(before,after) && sameAnchor(before,after,"commands")); xrtValueRelease(after); after=NULL;
    // Tree-only changes participate in the debounced checkpoint.
    CHECK(xuiTreeViewSetNodeExpanded(app.commands,3,0)==XUI_OK); xideSessionPoll(&app); Sleep(850); xideSessionPoll(&app);
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error)); invalid=xrtValueDeepClone(get(state.source,"reading")); CHECK(invalid);
    CHECK(xrtValueObjectSetNew(get(invalid,"commands"),sv("fraction"),xrtValueFloat(-0.5)) && !xideReadingValid(invalid));
    after=xideReadingCapture(&app,NULL); CHECK(after && !xideReadingRestore(&app,invalid,0)); xvalue* unchanged=xideReadingCapture(&app,NULL); CHECK(unchanged && xrtValueEqual(after,unchanged)); xrtValueRelease(unchanged); xrtValueRelease(after); after=NULL; xrtValueRelease(invalid); invalid=NULL;
    invalid=xrtValueDeepClone(get(state.source,"reading")); CHECK(invalid && xrtValueArrayAppend(get(get(invalid,"commands"),"nodes"),xrtValueArrayGet(get(get(invalid,"commands"),"nodes"),0)) && !xideReadingValid(invalid));
    after=xrtValueDeepClone(state.source); CHECK(after && xrtValueObjectSetNew(after,sv("reading"),invalid)); invalid=NULL; CHECK(xideStateWrite(&state,after,&error)); xrtValueRelease(after); after=NULL; xideStateClear(&state); xideAppDestroy(&app);
    size_t bytes=0; raw=xrtFileReadAllLimit(metadata,8u*1024u*1024u,&bytes); CHECK(raw);
    CHECK(start(&app,storage,manifest,settings,catalogPath) && settle(&app) && !app.activeDocument && app.documents.count==0 && xideSessionIssue(&app)[0] && !xideSessionSave(&app,0,&error));
    size_t preservedBytes=0; unsigned char* preserved=xrtFileReadAllLimit(metadata,8u*1024u*1024u,&preservedBytes); CHECK(preserved && preservedBytes==bytes && !memcmp(raw,preserved,bytes)); xrtFree(preserved);
    // Legacy session-1 files without reading metadata remain valid and gain it
    // on their next successful checkpoint; this is an isolated fixture repair.
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error)); invalid=xrtValueDeepClone(state.source);
    CHECK(invalid && xrtValueObjectRemove(invalid,sv("reading")) && xideStateWrite(&state,invalid,&error));
    xrtValueRelease(invalid); invalid=NULL; xideStateClear(&state); xideAppDestroy(&app);
    CHECK(start(&app,storage,manifest,settings,catalogPath) && settle(&app) && app.activeDocument && !strcmp(app.activeDocument->language,"xlang") && !xideSessionIssue(&app)[0]);
    CHECK(xideSessionSave(&app,0,&error) && xideStateOpen(&state,metadata,"xlang-ide-session-1",&error) && xideReadingValid(get(state.source,"reading"))); xideStateClear(&state);
    puts("reading_test passed: native stable tree identities and selection after node reorder, collapsed branches, row anchors across UI font changes, long-help block/chapter anchors across reflow and restart, workspace isolation and property owner, Unicode paths, unknown fields, tree-only autosave, missing identities, explicit chapter override, document end, malformed/duplicate protection without partial restore, legacy session-1 compatibility, C/xlang state and real screenshots.");
cleanup:
    if(began) { xgeEnd(); } xideAppDestroy(&app); xideStateClear(&state); xideWorkspaceClear(&workspace); xideProjectClear(&project); xrtValueRelease(before); xrtValueRelease(after); xrtValueRelease(invalid); xrtFree(raw); xrtFree(fileKey); xrtFree(moduleKey); free(font); free(catalogPath);
    free(root); free(storage); free(settings); free(entry); free(native); free(projectPath); free(otherProject); free(manifest); free(other); free(note); free(metadata); free(firstImage); free(secondImage); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{
    fixture* f=user; if(!SetThreadDesktop(f->desktop)) return 1;
    xge_desc_t desc={0}; desc.iWidth=1600; desc.iHeight=1000; desc.sTitle="xlang IDE reading-state verification"; desc.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; desc.iRunMode=XGE_RUN_GAME_LOOP;
    int result=xgeInit(&desc); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1;
}
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;
    fixture f={0}; f.directory=argv[1]; wchar_t name[100]; swprintf(name,100,L"XlangIDEReadingVerification-%lu",GetCurrentProcessId());
    f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) return 1;
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1;
    if(thread) { if(WaitForSingleObject(thread,60000)!=WAIT_OBJECT_0) { fputs("reading_test timed out.\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); }
    CloseDesktop(f.desktop); return (int)code;
}
