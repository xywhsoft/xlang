#include "services/file_sync.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/file_changes_view.h"
#include "theme/theme.h"
#include "platform/path.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"file_sync_test:%d: %s; %s; changes=%zu\n",__LINE__,#v,error.message,xideFileSyncCount(&app)); failed=1; goto cleanup; } } while(0)
static int writeText(const char* p,const char* s) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,strlen(s)}); }
static xvalue* get(const xvalue* v,const char* key) { return xrtValueObjectGet(v,(xstrview){key,strlen(key)}); }
static int set(xvalue* v,const char* key,const char* text) { return xrtValueObjectSetNew(v,(xstrview){key,strlen(key)},xrtValueString((xstrview){text,strlen(text)})); }
static int renameProject(xvalue* root,const char* name)
{ xvalue* ide=xrtValueClone(get(root,"ide")); int okay=ide && set(ide,"name",name); if(okay) okay=xrtValueObjectSetNew(root,(xstrview){"ide",3},ide); else xrtValueRelease(ide); return okay; }
static int writeValue(const char* p,const xvalue* v) { size_t n=0; char* s=xrtJsonStringify(v,true,&n); int okay=s && xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)s,n}); xrtFree(s); return okay; }
static int settle(xide_app* app)
{ if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0; xideEditorPoll(app); xideFileSyncPoll(app); xideWorkbenchLayout(app); return xuiLayout(app->context)==XUI_OK; }
static int idle(xide_app* app)
{ uint64_t end=GetTickCount64()+12000; do { if(!settle(app)) return 0; if(xideFileSyncSettled(app)) return 1; Sleep(10); } while(GetTickCount64()<end); return 0; }
static int waitText(xide_app* app,xide_document* d,const char* s)
{ uint64_t end=GetTickCount64()+12000; do { if(!settle(app)) return 0; if(!strcmp(xuiCodeEditGetText(d->editor),s)) return 1; Sleep(10); } while(GetTickCount64()<end); return 0; }
static const xide_file_change* change(xide_app* app,const char* path)
{ for(size_t i=0;i<xideFileSyncCount(app);++i) { const xide_file_change* c=xideFileSyncAt(app,i); if(c && !strcmp(c->snapshot.path,path)) return c; } return NULL; }
static int waitChange(xide_app* app,const char* path)
{ uint64_t end=GetTickCount64()+12000; do { if(!settle(app)) return 0; if(change(app,path)) return 1; Sleep(10); } while(GetTickCount64()<end); return 0; }
static int waitReadyChange(xide_app* app,const char* path,const char* part)
{ uint64_t end=GetTickCount64()+12000; do { if(!settle(app)) return 0; const xide_file_change* c=change(app,path); if(c && c->snapshot.state==XIDE_WATCH_READY && c->snapshot.bytes && strstr((const char*)c->snapshot.bytes,part)) return 1; Sleep(10); } while(GetTickCount64()<end); return 0; }
static int waitName(xide_app* app,const char* id,const char* name)
{ uint64_t end=GetTickCount64()+12000; do { if(!settle(app)) return 0; xide_project* p=xideWorkspaceFindProject(&app->workspaceModel,id); if(p && !strcmp(p->name,name)) return 1; Sleep(10); } while(GetTickCount64()<end); return 0; }
static xui_widget findType(xui_widget w,xui_widget_type type)
{ if(xuiWidgetGetType(w)==type) return w; for(xui_widget c=xuiWidgetGetFirstChild(w);c;c=xuiWidgetGetNextSibling(c)) { xui_widget result=findType(c,type); if(result) return result; } return NULL; }
int main(int argc,char** argv)
{
    int failed=0; xide_app app={0}; xide_model_error error={0}; xide_project project={0}; xide_workspace workspace={0}; xvalue* value=NULL; char* serialized=NULL;
    char *root=NULL,*a=NULL,*b=NULL,*third=NULL,*manifest=NULL,*workspacePath=NULL; wchar_t w[4096];
    CHECK(argc==2); char* id=xideCreateId("sync-test"); root=id ? xidePathResolve(argv[1],id) : NULL; free(id); CHECK(root && xrtDirCreateAll(root));
    a=xidePathResolve(root,"中文 main.xl"); b=xidePathResolve(root,"helper.c"); third=xidePathResolve(root,"extra.xlp"); manifest=xidePathResolve(root,"app.xlp"); workspacePath=xidePathResolve(root,"work.xlw");
    const char* xl="func main(...) as int\n    return 0\nend\n"; const char* c="int value = 1; // 中文😀\n";
    CHECK(a && b && third && manifest && workspacePath && writeText(a,xl) && writeText(b,c));
    CHECK(xideProjectCreate(&project,manifest,"Original",a,&error) && xideProjectAddFile(&project,b,NULL,&error) && xideProjectSave(&project,NULL,&error));
    CHECK(xideProjectCreate(&project,third,"Third",a,&error) && xideProjectSave(&project,NULL,&error));
    CHECK(xideWorkspaceCreate(&workspace,"Workspace",&error) && xideWorkspaceAddProject(&workspace,manifest,"app",&error) && xideWorkspaceSetStartup(&workspace,"app",&error) && xideWorkspaceSave(&workspace,workspacePath,&error));
    app.hidden=1; app.noSession=1; app.viewportWidth=1280; app.viewportHeight=800;
    CHECK(xuiCreate(&app.context)==XUI_OK && ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK && xuiInputViewport(app.context,1280,800)==XUI_OK && xideWorkbenchCreate(&app)==XUI_OK);
    CHECK(xideWorkspaceOpen(&app,workspacePath,0,&error) && xideEditorOpen(&app,a,&error)); xide_document* xd=app.activeDocument;
    CHECK(xideEditorOpen(&app,b,&error)); xide_document* cd=app.activeDocument; uint64_t cid=cd->id,xid=xd->id;
    CHECK(xideFileSyncStart(&app,&error) && idle(&app) && !xideFileSyncCount(&app));
    CHECK(writeText(b,"int value = 2; // 外部😀\n") && waitText(&app,cd,"int value = 2; // 外部😀\n") && !cd->dirty && cd->id==cid && !strcmp(cd->language,"c"));
    xui_code_document core=xuiCodeEditGetDocument(cd->editor);
    CHECK(xuiCodeDocumentUndo(core)==XUI_OK && cd->dirty && !strcmp(xuiCodeEditGetText(cd->editor),c));
    CHECK(xuiCodeDocumentRedo(core)==XUI_OK && !cd->dirty);
    /* Keeping a clean deferred snapshot remains effective across resubscriptions. */
    CHECK(xideFileChangesViewOpen(&app) && writeText(b,"int clean deferred = 7;\n") && waitReadyChange(&app,b,"clean deferred"));
    const xide_file_change* keptClean=change(&app,b); CHECK(keptClean && xideFileSyncKeep(&app,keptClean->snapshot.token,keptClean->snapshot.sequence));
    CHECK(xideDialogSetOpen(xideFileChangesViewWindow(&app),0)==XUI_OK);
    CHECK(xideEditorOpen(&app,third,&error) && idle(&app));
    CHECK(!change(&app,b) && !strcmp(xuiCodeEditGetText(cd->editor),"int value = 2; // 外部😀\n") && !cd->dirty && xideDocumentCheckExternal(cd));
    CHECK(writeText(b,"int next clean = 8;\n") && waitText(&app,cd,"int next clean = 8;\n"));
    CHECK(xuiCodeFoldStateFoldAll(xuiCodeEditGetFoldState(xd->editor))==XUI_OK);
    CHECK(writeText(a,"func main(...) as int\n    return 9\nend\n") && waitText(&app,xd,"func main(...) as int\n    return 9\nend\n") && xd->id==xid && !strcmp(xd->language,"xlang"));
    int visible=1; CHECK(xuiCodeFoldStateIsLineVisible(xuiCodeEditGetFoldState(xd->editor),1,&visible)==XUI_OK && !visible);
    CHECK(xuiCodeDocumentInsert(core,0,"// local\n")==XUI_OK && writeText(b,"int disk = 3;\n") && waitReadyChange(&app,b,"disk = 3") && cd->dirty && strstr(xuiCodeEditGetText(cd->editor),"// local"));
    const xide_file_change* pending=change(&app,b); uint64_t token=pending->snapshot.token,sequence=pending->snapshot.sequence; uint32_t version=xuiCodeDocumentGetVersion(core);
    CHECK(xideFileSyncKeep(&app,token,sequence) && !change(&app,b) && xideDocumentCheckExternal(cd) && !xideDocumentSave(cd,NULL,0,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(xideFileChangesViewOpen(&app));
    value=xrtValueClone(xideWorkspaceFindProject(&app.workspaceModel,"app")->source); CHECK(value && renameProject(value,"Kept model") && writeValue(manifest,value)); xrtValueRelease(value); value=NULL;
    CHECK(waitReadyChange(&app,manifest,"Kept model")); const xide_file_change* keptModel=change(&app,manifest); CHECK(keptModel && xideFileSyncKeep(&app,keptModel->snapshot.token,keptModel->snapshot.sequence));
    CHECK(xideDialogSetOpen(xideFileChangesViewWindow(&app),0)==XUI_OK);
    CHECK(xideEditorOpen(&app,manifest,&error) && idle(&app) && !change(&app,b)); xide_document* md=app.activeDocument;
    CHECK(!strcmp(xideWorkspaceFindProject(&app.workspaceModel,"app")->name,"Original") && strstr(md->baseline.text,"Kept model") && !change(&app,manifest));
    CHECK(writeText(b,"int disk = 4;\n") && waitReadyChange(&app,b,"disk = 4")); pending=change(&app,b);
    token=pending->snapshot.token; sequence=pending->snapshot.sequence; version=xuiCodeDocumentGetVersion(core);
    CHECK(!xideFileSyncReload(&app,token,sequence,version,0,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(xuiCodeDocumentInsert(core,0,"// second local\n")==XUI_OK && !xideFileSyncReload(&app,token,sequence,version,1,&error));
    version=xuiCodeDocumentGetVersion(core); CHECK(xideFileSyncReload(&app,token,sequence,version,1,&error) && !cd->dirty && !strcmp(xuiCodeEditGetText(cd->editor),"int disk = 4;\n"));
    CHECK(xuiCodeDocumentUndo(core)==XUI_OK && cd->dirty && strstr(xuiCodeEditGetText(cd->editor),"// second local")); CHECK(xuiCodeDocumentRedo(core)==XUI_OK && !cd->dirty);
    const unsigned char bad[]={0xff,0}; CHECK(xrtFileWriteAtomic(b,(xbytesview){bad,sizeof(bad)}) && waitChange(&app,b) && !strcmp(xuiCodeEditGetText(cd->editor),"int disk = 4;\n"));
    CHECK(writeText(b,"int repaired = 5;\n") && waitText(&app,cd,"int repaired = 5;\n") && !change(&app,b));
    CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,b,-1,w,4096) && DeleteFileW(w) && waitChange(&app,b) && cd->open && cd->id==cid);
    CHECK(writeText(b,"int recreated = 6;\n") && waitText(&app,cd,"int recreated = 6;\n") && !change(&app,b));
    /* Model text save immediately updates the bound model, without closing other documents. */
    xui_code_document manifestCore=xuiCodeEditGetDocument(md->editor);
    CHECK(xuiCodeDocumentInsert(manifestCore,0,"broken")==XUI_OK && !xideDocumentSave(md,NULL,0,&error) && error.code==XIDE_MODEL_INVALID && md->dirty);
    CHECK(xuiCodeDocumentUndo(manifestCore)==XUI_OK && !md->dirty);
    value=xrtValueClone(xideWorkspaceFindProject(&app.workspaceModel,"app")->source); CHECK(value && renameProject(value,"Saved text"));
    size_t length=0; serialized=xrtJsonStringify(value,true,&length); CHECK(serialized && xuiCodeDocumentReplace(manifestCore,0,xuiCodeDocumentGetLength(manifestCore),serialized)==XUI_OK); CHECK(!strcmp(xuiCodeEditGetText(md->editor),serialized));
    xvalue* direct=xrtJsonParse((xstrview){serialized,strlen(serialized)}); CHECK(direct && xrtValueIs(direct,XVALUE_OBJECT)); xrtValueRelease(direct);
    xrtFree(serialized); serialized=NULL;
    memset(&error,0,sizeof(error)); CHECK(xideDocumentSave(md,NULL,0,&error));
    CHECK(!strcmp(xideWorkspaceFindProject(&app.workspaceModel,"app")->name,"Saved text") && cd->open && xd->open && cid==cd->id && xid==xd->id);
    xrtValueRelease(value); value=NULL; CHECK(idle(&app));
    CHECK(xideWorkspaceViewSelectProject(&app,"app")); int selected=xuiTreeViewGetSelected(app.workspace);
    value=xrtValueClone(xideWorkspaceFindProject(&app.workspaceModel,"app")->source); CHECK(value && renameProject(value,"External model") && writeValue(manifest,value)); xrtValueRelease(value); value=NULL;
    CHECK(waitName(&app,"app","External model") && strstr(xuiCodeEditGetText(md->editor),"External model") && xuiTreeViewGetSelected(app.workspace)==selected && !strcmp(app.selectedProject,"app") && !strcmp(app.workspaceModel.startupProject,"app"));
    CHECK(writeText(manifest,"{invalid") && waitChange(&app,manifest) && !strcmp(xideWorkspaceFindProject(&app.workspaceModel,"app")->name,"External model") && !strstr(xuiCodeEditGetText(md->editor),"{invalid"));
    value=xrtValueClone(xideWorkspaceFindProject(&app.workspaceModel,"app")->source); CHECK(value && writeValue(manifest,value)); xrtValueRelease(value); value=NULL; xideFileSyncRefresh(&app); CHECK(idle(&app));
    CHECK(xideWorkspaceViewSelectProject(&app,"app")); xui_widget name=findType(app.properties,xuiWidgetFindType(app.context,"input")); CHECK(name && xuiEditSetText(name,"Unapplied property draft")==XUI_OK && xideWorkspaceViewHasDraft(&app));
    value=xrtValueClone(xideWorkspaceFindProject(&app.workspaceModel,"app")->source); CHECK(value && renameProject(value,"Deferred model") && writeValue(manifest,value)); xrtValueRelease(value); value=NULL;
    CHECK(waitChange(&app,manifest) && !strcmp(xuiEditGetText(name),"Unapplied property draft") && !strcmp(xideWorkspaceFindProject(&app.workspaceModel,"app")->name,"External model"));
    CHECK(xuiEditSetText(name,"External model")==XUI_OK && waitName(&app,"app","Deferred model"));
    /* Matching workspace references retain dirty project models; removal needs consent. */
    CHECK(xideProjectSetMetadata(xideWorkspaceFindProject(&app.workspaceModel,"app"),"Dirty retained","","exe",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK);
    value=xrtValueClone(app.workspaceModel.source); CHECK(value && set(value,"name","Renamed workspace"));
    xvalue* refs=xrtValueClone(get(value,"projects")); CHECK(refs && xrtValueObjectSetNew(value,(xstrview){"projects",8},refs));
    xvalue* ref=xrtValueObject(); CHECK(ref && set(ref,"id","third") && set(ref,"path","extra.xlp") && xrtValueArrayAppendNew(get(value,"projects"),ref) && writeValue(workspacePath,value)); xrtValueRelease(value); value=NULL;
    uint64_t due=GetTickCount64()+12000; do { CHECK(settle(&app)); if(app.workspaceModel.projectCount==2 && xideWorkspaceFindProject(&app.workspaceModel,"third")->available) break; Sleep(10); } while(GetTickCount64()<due);
    CHECK(app.workspaceModel.projectCount==2 && xideWorkspaceFindProject(&app.workspaceModel,"third")->available && !strcmp(app.workspaceModel.name,"Renamed workspace") && xideWorkspaceFindProject(&app.workspaceModel,"app")->dirty && !strcmp(xideWorkspaceFindProject(&app.workspaceModel,"app")->name,"Dirty retained"));
    const char* removed="{\"format\":\"xlang-workspace-1\",\"name\":\"Removed\",\"projects\":[{\"id\":\"third\",\"path\":\"extra.xlp\"}]}";
    CHECK(writeText(workspacePath,removed) && waitChange(&app,workspacePath) && app.workspaceModel.projectCount==2);
    pending=change(&app,workspacePath); token=pending->snapshot.token; sequence=pending->snapshot.sequence;
    CHECK(xideFileSyncReload(&app,token,sequence,0,1,&error) && app.workspaceModel.projectCount==1 && !xideWorkspaceFindProject(&app.workspaceModel,"app") && cd->open && xd->open && md->open);
    CHECK(idle(&app));
    CHECK(xuiCodeDocumentInsert(core,0,"// lifetime unsaved\n")==XUI_OK && writeText(b,"int lifetime external = 10;\n") && waitChange(&app,b));
    CHECK(strstr(xuiStatusBarGetItem(app.statusBar,0)->sText,"磁盘变化"));
    xideFileSyncStop(&app); CHECK(!app.fileSync && !strcmp(xuiStatusBarGetItem(app.statusBar,0)->sText,"就绪") && settle(&app));
    CHECK(xideFileSyncStart(&app,&error) && idle(&app));
    puts("file_sync_test passed: real worker-to-editor C/xlang reload and undo, conflicts/keep/new versions/stale consent, invalid encodings/missing/recreated files, immediate validated manifest text save, valid/invalid external model transactions, stable selection and document identities, unapplied property draft protection, reference-only workspace merge and consent for dirty project removal");
cleanup:
    xrtValueRelease(value); xrtFree(serialized); xideAppDestroy(&app); xideProjectClear(&project); xideWorkspaceClear(&workspace); free(root); free(a); free(b); free(third); free(manifest); free(workspacePath); return failed;
}
