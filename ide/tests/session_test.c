#include "services/session.h"
#include "ui/layout_state.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "ui/catalog_view.h"
#include "platform/path.h"
#include "platform/state_file.h"
#include "editor/coordinates.h"
#include "theme/theme.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do { if(!(e)) { fprintf(stderr,"session_test:%d: %s; %s\n",__LINE__,#e,error.message); failed=1; goto cleanup; } } while(0)
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* key) { return xrtValueObjectGet(v,sv(key)); }
static const char* text(const xvalue* v) { xstrview s={0}; return xrtValueGetString(v,&s) ? s.Data : ""; }
static int writeRaw(const char* path,const unsigned char* bytes,size_t count)
{ char* native=xidePathNativeUtf8(path); int okay=native && xrtFileWriteAtomic(native,(xbytesview){bytes,count}); free(native); return okay; }
static int writeText(const char* path,const char* text) { return writeRaw(path,(const unsigned char*)text,strlen(text)); }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int start(xide_app* app,const char* storage,const char* manifest,int shift)
{
    xide_model_error error={0}; memset(app,0,sizeof(*app)); app->hidden=1; app->viewportWidth=1280; app->viewportHeight=800; app->sessionRoot=storage;
    if(xuiCreate(&app->context) || ideThemeInit(app->context,0) || xuiInputViewport(app->context,1280,800) || xideWorkbenchCreate(app)) return 0;
    if(shift) { xui_widget placeholder=NULL; int window=-1; if(xuiWidgetCreate(app->context,&placeholder) || xuiDockPanelAddWindow(app->dock,"Unknown runtime window",placeholder,&window)) return 0; }
    if(manifest && !xideWorkspaceOpen(app,manifest,0,&error)) return 0;
    return xideSessionStart(app);
}
static int settle(xide_app* app)
{ xideWorkbenchLayout(app); if(xuiDispatchPendingEvents(app->context) || xuiLayout(app->context)) return 0; xideSessionAfterLayout(app); return 1; }
static int windowState(xide_app* app,int window,int expected)
{ xui_dock_window_info_t info={0}; info.iSize=sizeof(info); return !xuiDockPanelGetWindowInfo(app->dock,window,&info) && info.iState==expected; }
static HANDLE lockFile(const char* path)
{ wchar_t* wide=xidePathNativeWide(path); HANDLE h=wide ? CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL) : INVALID_HANDLE_VALUE; free(wide); return h; }
int main(int argc,char** argv)
{
    xide_app app={0}; xide_workspace workspace={0}; xide_project project={0}; xide_state_file state={0}; xide_model_error error={0};
    char *root=NULL,*storage=NULL,*a=NULL,*b=NULL,*manifest=NULL,*other=NULL,*projectPath=NULL,*directory=NULL,*metadata=NULL,*layoutPath=NULL,*savedId=NULL;
    xvalue *logical=NULL,*invalid=NULL,*retained=NULL; xide_recovery_guard checkpointGuard={0}; char *previousPath=NULL,*firstRecovery=NULL; HANDLE locked=INVALID_HANDLE_VALUE; int failed=0;
    CHECK(argc==2 || (argc==3 && !strcmp(argv[2],"--deep"))); char* id=xideCreateId("session"); root=id ? xidePathResolve(argv[1],id) : NULL; free(id); CHECK(root && xrtDirCreateAll(root));
    storage=xidePathResolve(root,"会话"); a=xidePathResolve(root,"main.xl"); b=xidePathResolve(root,"native.c"); manifest=xidePathResolve(root,"a.xlw"); other=xidePathResolve(root,"b.xlw"); projectPath=xidePathResolve(root,"project.xlp");
    CHECK(storage && a && b && manifest && other && projectPath);
    if(argc==3) {
        for(int i=0;i<32;++i) { wchar_t* w=xidePathNativeWide(storage); size_t units=w ? wcslen(w) : 0; free(w); CHECK(units);
            if(units>=400) break;
            char* next=xidePathResolve(storage,"中文😀-long-session-storage"); CHECK(next); free(storage); storage=next;
        }
        wchar_t* w=xidePathNativeWide(storage); CHECK(w && wcslen(w)>=400); free(w);
    }
    CHECK(writeText(a,"func main(...) as int\n    return 0\nend\n") && writeText(b,"int helper(void) {\n    // body\n    return 42;\n}\n"));
    CHECK(xideProjectCreate(&project,projectPath,"双语法恢复",a,&error) && xideProjectSave(&project,NULL,&error));
    CHECK(xideWorkspaceCreate(&workspace,"恢复测试",&error) && xideWorkspaceAddProject(&workspace,projectPath,"main",&error) && xideWorkspaceSave(&workspace,manifest,&error) && xideWorkspaceSave(&workspace,other,&error));
    CHECK(start(&app,storage,manifest,0) && settle(&app));
    CHECK(xideEditorOpen(&app,a,&error)); xide_document* first=app.activeDocument; int firstWindow=first->window;
    const char* recovered="//头\n中😀abc\n";
    CHECK(xuiCodeDocumentReplace(xuiCodeEditGetDocument(first->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(first->editor)),recovered)==XUI_OK && first->dirty);
    CHECK(xideDocumentSetLanguage(first,"c",&error)); savedId=copy(first->sessionId); CHECK(savedId);
    CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(first->editor),xuiCodeEditGetDocument(first->editor),xidePositionToOffset(recovered,2,2),xidePositionToOffset(recovered,2,3))==XUI_OK);
    CHECK(xideEditorOpen(&app,b,&error)); xide_document* second=app.activeDocument;
    CHECK(!xuiCodeFoldStateFoldAll(xuiCodeEditGetFoldState(second->editor)));
    xide_document* untitled=xideDocumentNew(&app.documents,"xlang",&error); CHECK(untitled && !xideEditorShow(&app,untitled));
    CHECK(!xuiCodeDocumentInsert(xuiCodeEditGetDocument(untitled->editor),0,"// 未存😀\n"));
    for(int i=0;i<60;++i) CHECK(!xuiCodeDocumentInsert(xuiCodeEditGetDocument(untitled->editor),xuiCodeDocumentGetLength(xuiCodeEditGetDocument(untitled->editor)),"// 恢复长文档\n"));
    CHECK(!xuiDockPanelFloatWindow(app.dock,untitled->window,(xui_rect_t){220,180,430,260}));
    CHECK(!xuiDockPanelAutoHideWindow(app.dock,app.windows[XIDE_COMMANDS]) && !xuiDockPanelHideWindow(app.dock,app.windows[XIDE_PROPERTIES]));
    CHECK(!xideWorkbenchApplyTheme(&app,IDE_THEME_GRAPHITE) && !xideCatalogViewSearch(&app,"string split"));
    CHECK(!xideEditorShow(&app,first) && settle(&app));
    CHECK(!xuiCodeEditSetScroll(untitled->editor,0,140)); float restoredScroll=0; CHECK(!xuiCodeEditGetScroll(untitled->editor,NULL,&restoredScroll) && restoredScroll>0);
    CHECK(xideSessionSave(&app,0,&error));
    directory=copy(xideSessionDirectory(&app)); metadata=xidePathResolve(directory,"session.json"); layoutPath=xidePathResolve(directory,"layout.json"); CHECK(directory && metadata && layoutPath);
    previousPath=xideRecoveryPath(directory,"previous-session.json",&error); CHECK(previousPath);
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error));
    firstRecovery=copy(text(get(xrtValueArrayGet(get(state.source,"openDocuments"),0),"recoveryText"))); CHECK(firstRecovery && *firstRecovery);
    xideStateClear(&state);
    CHECK(xideRecoveryLock(&checkpointGuard,directory,&error));
    CHECK(!xuiCodeDocumentInsert(xuiCodeEditGetDocument(first->editor),0,"// pending checkpoint\n"));
    CHECK(!xideSessionSave(&app,0,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error) && !strcmp(firstRecovery,text(get(xrtValueArrayGet(get(state.source,"openDocuments"),0),"recoveryText"))));
    xideStateClear(&state); xideRecoveryUnlock(&checkpointGuard);
    CHECK(xideSessionSave(&app,0,&error));
    CHECK(xideStateOpen(&state,previousPath,"xlang-ide-session-1",&error) && !strcmp(firstRecovery,text(get(xrtValueArrayGet(get(state.source,"openDocuments"),0),"recoveryText"))));
    retained=xrtValueDeepClone(state.source); CHECK(retained); xideStateClear(&state);
    CHECK(writeText(previousPath,"{damaged previous checkpoint"));
    CHECK(!xuiCodeDocumentInsert(xuiCodeEditGetDocument(first->editor),0,"// latest edits must still save\n") && xideSessionSave(&app,0,&error));
    xide_recovery_stats recoveryStats={0}; CHECK(!xideSessionMaintain(&app,&recoveryStats,&error) && recoveryStats.removed==0);
    size_t retainedSize=0; char* retainedJson=xrtJsonStringify(retained,true,&retainedSize); CHECK(retainedJson);
    CHECK(writeRaw(previousPath,(const unsigned char*)retainedJson,retainedSize)); xrtFree(retainedJson); xrtValueRelease(retained); retained=NULL;
    CHECK(!xuiCodeDocumentReplace(xuiCodeEditGetDocument(first->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(first->editor)),recovered));
    CHECK(!xuiCodeSelectionSetRange(xuiCodeEditGetSelection(first->editor),xuiCodeEditGetDocument(first->editor),xidePositionToOffset(recovered,2,2),xidePositionToOffset(recovered,2,3)) && xideSessionSave(&app,0,&error));
    CHECK(xideSessionMaintain(&app,&recoveryStats,&error));
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error)); xvalue* docs=get(state.source,"openDocuments"); CHECK(xrtValueCount(docs)==3 && !strcmp(text(get(xrtValueArrayGet(docs,0),"path")),"main.xl"));
    CHECK(get(xrtValueArrayGet(docs,0),"recoveryText") && !get(xrtValueArrayGet(docs,1),"recoveryText") && get(xrtValueArrayGet(docs,2),"recoveryText"));
    CHECK(!strcmp(text(get(state.source,"coordinateSystem")),"unicode-codepoint-1"));
    invalid=xrtValueDeepClone(state.source); CHECK(invalid && xrtValueObjectSetNew(invalid,sv("future"),xrtJsonParse(sv("{\"keep\":true}"))));
    CHECK(xrtValueObjectSetNew(xrtValueArrayGet(get(invalid,"configurations"),0),sv("futureConfig"),xrtValueString(sv("keep"))));
    CHECK(xideStateWrite(&state,invalid,&error)); xrtValueRelease(invalid); invalid=NULL; xideStateClear(&state);
    CHECK(xideStateOpen(&state,layoutPath,"xlang-ide-layout-1",&error)); invalid=xrtValueDeepClone(state.source); CHECK(invalid);
    CHECK(xrtValueObjectSetNew(get(xrtValueArrayGet(get(invalid,"states"),0),"dock"),sv("futureDock"),xrtValueString(sv("keep"))));
    CHECK(xideStateWrite(&state,invalid,&error)); xrtValueRelease(invalid); invalid=NULL; xideStateClear(&state);
    // The external addition requires an explicit reopen rather than silent overwrite.
    CHECK(!xideSessionSave(&app,0,&error) && error.code==XIDE_MODEL_CONFLICT);
    CHECK(writeText(a,"// external disk change\n")); xideAppDestroy(&app);
    CHECK(start(&app,storage,manifest,1) && settle(&app));
    first=xideDocumentsFind(&app.documents,a); second=xideDocumentsFind(&app.documents,b); CHECK(first && second && first->window!=firstWindow && app.activeDocument==first);
    CHECK(first->dirty && !strcmp(first->sessionId,savedId) && !strcmp(first->language,"c") && first->languageOverride && !strcmp(xuiCodeEditGetText(first->editor),recovered));
    xui_code_selection_t selection={0}; CHECK(!xuiCodeSelectionGetState(xuiCodeEditGetSelection(first->editor),&selection));
    CHECK(selection.iCaretOffset==xidePositionToOffset(recovered,2,3) && selection.iAnchorOffset==xidePositionToOffset(recovered,2,2));
    CHECK(xideDocumentCheckExternal(first) && !xideDocumentSave(first,NULL,0,&error) && error.code==XIDE_MODEL_CONFLICT);
    untitled=NULL; for(size_t i=0;i<app.documents.count;++i) if(app.documents.items[i]->open && !app.documents.items[i]->path) untitled=app.documents.items[i];
    CHECK(untitled && untitled->dirty && strstr(xuiCodeEditGetText(untitled->editor),"// 未存😀\n"));
    float scroll=0; CHECK(!xuiCodeEditGetScroll(untitled->editor,NULL,&scroll) && scroll==restoredScroll);
    int visible=1; CHECK(!xuiCodeFoldStateIsLineVisible(xuiCodeEditGetFoldState(second->editor),1,&visible) && !visible);
    CHECK(windowState(&app,untitled->window,XUI_DOCK_PANEL_WINDOW_FLOATING) && windowState(&app,app.windows[XIDE_COMMANDS],XUI_DOCK_PANEL_WINDOW_AUTO_HIDE) && windowState(&app,app.windows[XIDE_PROPERTIES],XUI_DOCK_PANEL_WINDOW_HIDDEN));
    CHECK(app.theme==IDE_THEME_GRAPHITE && !strcmp(xuiEditGetText(xideCatalogSearchWidget(&app)),"string split"));
    logical=xideLayoutCapture(&app,&error); CHECK(logical); invalid=xrtValueDeepClone(logical); CHECK(invalid);
    xvalue* right=xrtValueArrayGet(get(invalid,"regions"),0); CHECK(xrtValueObjectSetNew(get(right,"root"),sv("axis"),xrtValueString(sv("invalid-axis"))));
    CHECK(!xideLayoutRestore(&app,invalid,&error)); xrtValueRelease(invalid); invalid=xideLayoutCapture(&app,&error); CHECK(invalid && xrtValueEqual(logical,invalid)); xrtValueRelease(invalid); invalid=NULL;
    xvalue* repaired=xrtValueDeepClone(logical); CHECK(repaired);
    xvalue* left=xrtValueArrayGet(get(repaired,"regions"),1); xvalue* rootNode=xrtJsonParse(sv("{\"type\":\"pane\",\"active\":\"removed-panel\",\"tabs\":[\"removed-panel\"]}")); CHECK(rootNode && xrtValueObjectSetNew(left,sv("root"),rootNode));
    xvalue* windows=get(repaired,"windows");
    for(size_t i=0;i<xrtValueCount(windows);++i) if(!strcmp(text(get(xrtValueArrayGet(windows,i),"id")),"commands")) CHECK(xrtValueObjectSetNew(xrtValueArrayGet(windows,i),sv("state"),xrtValueString(sv("docked"))));
    CHECK(xideLayoutRestore(&app,repaired,&error) && windowState(&app,app.windows[XIDE_COMMANDS],XUI_DOCK_PANEL_WINDOW_DOCKED)); xrtValueRelease(repaired);
    CHECK(xideLayoutRestore(&app,logical,&error));
    repaired=xrtValueDeepClone(logical); CHECK(repaired);
    left=xrtValueArrayGet(get(repaired,"regions"),1); CHECK(xrtValueObjectSetNew(left,sv("value"),xrtValueInt(190)));
    CHECK(xideLayoutRestore(&app,repaired,&error)); xrtValueRelease(repaired);
    CHECK(xideSessionSave(&app,0,&error));
    CHECK(xideStateOpen(&state,layoutPath,"xlang-ide-layout-1",&error) && get(get(xrtValueArrayGet(get(state.source,"states"),0),"dock"),"futureDock")); xideStateClear(&state);
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error) && get(xrtValueArrayGet(get(state.source,"configurations"),0),"futureConfig")); xideStateClear(&state);
    // Layout is published first; locked metadata must keep its referenced layout generation usable.
    locked=lockFile(metadata); CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(!xuiCodeDocumentInsert(xuiCodeEditGetDocument(first->editor),0,"// pending\n"));
    CHECK(!xuiDockPanelDockWindow(app.dock,app.windows[XIDE_COMMANDS],XUI_DOCK_PANEL_REGION_LEFT,XUI_DOCK_PANEL_SIDE_FILL,0,NULL));
    CHECK(!xideSessionSave(&app,0,&error)); CloseHandle(locked); locked=INVALID_HANDLE_VALUE; xideAppDestroy(&app);
    CHECK(start(&app,storage,manifest,0) && settle(&app)); first=xideDocumentsFind(&app.documents,a);
    CHECK(first && !strcmp(xuiCodeEditGetText(first->editor),recovered) && windowState(&app,app.windows[XIDE_COMMANDS],XUI_DOCK_PANEL_WINDOW_AUTO_HIDE));
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error) && get(state.source,"future")); xideStateClear(&state);
    // Same directory, different workspace; restore original only when returning.
    CHECK(xideWorkspaceOpen(&app,other,0,&error) && !app.activeDocument && strcmp(xideSessionDirectory(&app),directory));
    CHECK(xideEditorOpen(&app,b,&error) && xideSessionSave(&app,0,&error));
    CHECK(xideWorkspaceOpen(&app,manifest,0,&error) && xideDocumentsFind(&app.documents,a) && app.activeDocument);
    CHECK(xideSessionSave(&app,1,&error)); xideAppDestroy(&app);
    // A confirmed discard removes dirty recovery bodies from the next session.
    CHECK(start(&app,storage,manifest,0)); first=xideDocumentsFind(&app.documents,a); CHECK(first && !first->dirty && !strcmp(xuiCodeEditGetText(first->editor),"// external disk change\n"));
    for(size_t i=0;i<app.documents.count;++i) CHECK(!app.documents.items[i]->open || app.documents.items[i]->path);
    CHECK(xideSessionSave(&app,0,&error));
    CHECK(!xideWorkbenchApplyTheme(&app,IDE_THEME_PAPER)); xideSessionPoll(&app); Sleep(850); xideSessionPoll(&app);
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error) && !strcmp(text(get(state.source,"theme")),"warm-white")); xideStateClear(&state); xideAppDestroy(&app);
    // Last workspace pointer restores without explicit --workspace.
    CHECK(start(&app,storage,NULL,0) && xidePathEqual(app.workspaceModel.manifest,manifest));
    CHECK(xideWorkspaceOpen(&app,projectPath,1,&error) && !app.workspaceModel.manifest && !app.workspaceModel.dirty);
    CHECK(xideEditorOpen(&app,a,&error) && xideSessionSave(&app,0,&error)); xideAppDestroy(&app);
    CHECK(start(&app,storage,NULL,0) && !app.workspaceModel.manifest && app.projectSessionPath && xidePathEqual(app.projectSessionPath,projectPath) && xideDocumentsFind(&app.documents,a));
    first=app.activeDocument; CHECK(xideWorkspaceSaveAll(&app,other,&error) && app.activeDocument==first && first->open && xidePathEqual(app.workspaceModel.manifest,other));
    CHECK(xideWorkspaceOpen(&app,manifest,0,&error) && xideDocumentsFind(&app.documents,b) && xideSessionSave(&app,0,&error)); xideAppDestroy(&app);
    CHECK(xrtFileDelete(b)); CHECK(start(&app,storage,manifest,0) && !xideDocumentsFind(&app.documents,b) && xideSessionIssue(&app)[0] && xideSessionSave(&app,0,&error));
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error) && xrtValueCount(get(state.source,"openDocuments"))==2); xideStateClear(&state);
    CHECK(writeText(b,"int replacement = 7;\n") && xideEditorOpen(&app,b,&error) && xideSessionSave(&app,0,&error));
    CHECK(xideStateOpen(&state,metadata,"xlang-ide-session-1",&error) && xrtValueCount(get(state.source,"openDocuments"))==2); xideStateClear(&state); xideAppDestroy(&app);
    CHECK(writeText(metadata,"{broken"));
    CHECK(start(&app,storage,manifest,0) && !app.activeDocument && !xideSessionSave(&app,0,&error) && xideSessionIssue(&app)[0]);
    char* native=xidePathNativeUtf8(metadata); size_t size=0; unsigned char* raw=native ? xrtFileReadAllLimit(native,32,&size) : NULL; free(native); CHECK(raw && size==7 && !memcmp(raw,"{broken",7)); xrtFree(raw);
    xideAppDestroy(&app);
    CHECK(writeText(metadata,"{\"format\":\"xlang-ide-session-1\",\"openDocuments\":[{\"path\":\"main.xl\",\"id\":23}]}"));
    CHECK(start(&app,storage,manifest,0) && !app.activeDocument && !xideSessionSave(&app,0,&error));
    if(argc==3) puts("session_test deep Unicode storage passed: >=400 UTF-16 units, saved C/xlang/untitled recovery, previous checkpoint, locks/conflicts, reopen, discard, layout and corrupt-state preservation");
    puts("session_test passed: C/xlang recovery, Unicode selection, independent baseline, dirty/untitled snapshots, runtime-ID remapping, floating/autoHide/hidden layout, corrupt rollback, unknown fields, paired atomic generations, workspace isolation, scroll/folds, autosave, unavailable file preservation, standalone project, Save As, discard and last-workspace reopen");
cleanup:
    xideRecoveryUnlock(&checkpointGuard); xrtValueRelease(retained); free(previousPath); free(firstRecovery);
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    xrtValueRelease(logical); xrtValueRelease(invalid); xideStateClear(&state); xideAppDestroy(&app); xideProjectClear(&project); xideWorkspaceClear(&workspace);
    free(root); free(storage); free(a); free(b); free(manifest); free(other); free(projectPath); free(directory); free(metadata); free(layoutPath); free(savedId); return failed;
}
