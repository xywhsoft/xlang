#include "ui/editor_view.h"
#include "ui/language_view.h"
#include "ui/c_language_view.h"
#include "services/file_sync.h"
#include "services/file_rebind.h"
#include "ui/reading_state.h"
#include "ui/catalog_view.h"
#include "ui/completion_view.h"
#include "ui/c_completion_view.h"
#include "platform/path.h"
#include "theme/theme.h"
#include "ui/workspace_view.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) { fprintf(stderr,"editor call failed at %d: %s -> %d\n",__LINE__,#call,result_); return result_; } } while(0)
static void report(xide_app* app,const char* text)
{
    xideMessageShow(app,"xlang IDE",text,XUI_MSGBOX_ICON_ERROR);
}
static char* choosePath(xide_app* app,int save,xide_document* document)
{
    const char* extension=document && !strcmp(document->language,"xlang") ? "xl" : document && !strcmp(document->language,"c") ? "c" : "txt";
    return xideMessageChoosePath(app,save ? XUI_FILE_DIALOG_MODE_SAVE_FILE : XUI_FILE_DIALOG_MODE_OPEN_FILE,
        save ? "保存文件" : "打开文件","源代码和文本|*.xl;*.c;*.h;*.txt|所有文件|*.*",
        document ? document->path : NULL,save ? extension : NULL,
        document ? xideMessageDocumentKey(document,70,0) : UINT64_C(700),document ? document->id : 0);
}
static int saveDocument(xide_app* app,xide_document* document,int saveAs)
{
    if(!document || !document->open) return 0;
    char* path=saveAs || !document->path ? xideMessageSavePath(app,document->id) : NULL;
    if((saveAs || !document->path) && !path) {
        path=choosePath(app,1,document);
        if(path && !xideMessageRememberSavePath(app,document->id,path)) { free(path);return 0; }
    }
    if((saveAs || !document->path) && !path) return 0;
    int flags=path ? XIDE_DOCUMENT_OVERWRITE_TARGET : 0; xide_model_error error={0};
    int result=xideDocumentSave(document,path,flags,&error);
    if(!result && error.code==XIDE_MODEL_CONFLICT && document->path && (!path || xidePathEqual(path,document->path)) && xideDocumentCheckExternal(document) && xideMessageConfirm(app,"覆盖磁盘文件","保存目标在磁盘上发生变化。是否覆盖磁盘内容？",XUI_MSGBOX_BUTTON_YES_NO,xideMessageDocumentKey(document,1,1))==XUI_MSGBOX_RESULT_YES)
        result=xideDocumentSave(document,path,flags|XIDE_DOCUMENT_OVERWRITE_EXTERNAL,&error);
    free(path); if(!result) report(app,error.message);
    return result;
}
int xideEditorSaveForTask(xide_app* app,xide_document* document)
{ return document && document->open && ((!document->dirty && document->path) || saveDocument(app,document,0)); }
int xideEditorSaveForProject(xide_app* app,const xide_project* project)
{
    /* A manifest save can replace the workspace project array. Collect identities first. */
    char* root=xidePathDirectory(project->manifest); size_t* related=app->documents.count ? calloc(app->documents.count,sizeof(*related)) : NULL;
    if(!root || (app->documents.count && !related)) { free(root); free(related); return 0; }
    size_t length=strlen(root),count=0;
    for(size_t i=0;i<app->documents.count;++i) {
        xide_document* document=app->documents.items[i];
        if(!document->open || !document->dirty || !document->path) continue;
        int include=xidePathEqual(document->path,project->manifest) || xidePathEqual(document->path,project->entry);
        if(!include && !_strnicmp(document->path,root,length) && document->path[length]=='/') include=1;
        for(size_t j=0;j<project->files.count && !include;++j) include=xidePathEqual(document->path,project->files.items[j].path);
        if(include) related[count++]=i;
    }
    free(root); int okay=1;
    for(size_t i=0;i<count && okay;++i) okay=saveDocument(app,app->documents.items[related[i]],0);
    free(related); return okay;
}
static int refreshFindTables(xide_app* app,xui_widget widget)
{
    if(!widget) return XUI_OK;
    if(xuiWidgetGetType(widget)==xuiWidgetFindType(app->context,"tableview")) TRY(xuiTableViewRefresh(widget));
    for(xui_widget child=xuiWidgetGetFirstChild(widget);child;child=xuiWidgetGetNextSibling(child)) TRY(refreshFindTables(app,child));
    return XUI_OK;
}
/* The pinned SDK stores cross-editor offsets without document versions. Drop
   the whole result set before any changed/reused editor can expose old ranges. */
static int resetFindScope(xide_app* app)
{
    TRY(xuiCodeFindScopeClearEditors(app->editorFindScope));
    for(size_t i=0;i<app->documents.count;++i) {
        xide_document* d=app->documents.items[i];
        TRY(xuiCodeEditClearFind(d->editor));
        if(d->open && d->window>=0) {
            TRY(xuiCodeFindScopeAddEditorNamed(app->editorFindScope,d->editor,d->path ? d->path : d->title));
            d->findVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
        }
        TRY(refreshFindTables(app,xuiCodeEditGetFindWindow(d->editor)));
    }
    return XUI_OK;
}
static int syncFindScope(xide_app* app,xide_document* d)
{
    if(!app->editorFindScope) return XUI_OK;
    int member=xuiCodeEditGetFindScope(d->editor)==app->editorFindScope;
    if(!d->open) {
        if(member) TRY(resetFindScope(app));
        xui_widget window=xuiCodeEditGetFindWindow(d->editor);
        if(window) TRY(xuiWidgetSetVisible(window,0));
        return XUI_OK;
    }
    if(d->window<0) return XUI_OK;
    uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
    if(!member || (d->findVersion!=version && xuiCodeFindScopeGetResultCount(app->editorFindScope)>0)) {
        TRY(resetFindScope(app));
    } else {
        const char* name=d->path ? d->path : d->title;
        for(int i=0;i<xuiCodeFindScopeGetEditorCount(app->editorFindScope);++i) if(xuiCodeFindScopeGetEditor(app->editorFindScope,i)==d->editor) {
            if(strcmp(xuiCodeFindScopeGetEditorName(app->editorFindScope,i),name)) {
                TRY(xuiCodeFindScopeSetEditorName(app->editorFindScope,d->editor,name));
                for(size_t j=0;j<app->documents.count;++j) TRY(refreshFindTables(app,xuiCodeEditGetFindWindow(app->documents.items[j]->editor)));
            }
            break;
        }
    }
    d->findVersion=version;
    return XUI_OK;
}
static void documentChanged(xide_document* document,void* user)
{
    xide_app* app=user;
    if(!document->open && app->activeDocument==document) app->activeDocument=NULL;
    int findResult=syncFindScope(app,document); if(findResult!=XUI_OK) app->frameError=findResult;
    if(document->window>=0 && document->open) {
        size_t length=strlen(document->title)+3; char* title=malloc(length);
        if(title) { snprintf(title,length,"%s%s",document->title,document->dirty ? " *" : ""); xuiDockPanelSetWindowTitle(app->dock,document->window,title); free(title); }
    }
    if(document->dirty && document->path) xideTasksInvalidateInput(app->tasks,document->path);
    xideActionsRefresh(app); xgeRenderRequest();
}
static xide_document* byWindow(xide_app* app,int window)
{
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open && app->documents.items[i]->window==window) return app->documents.items[i];
    return NULL;
}
static void findActivated(xui_code_find_scope scope,xui_widget editor,const xui_code_find_result_t* result,void* user)
{
    (void)result; xide_app* app=user;
    if(scope!=app->editorFindScope) return;
    for(size_t i=0;i<app->documents.count;++i) {
        xide_document* d=app->documents.items[i];
        if(d->open && d->editor==editor) {
            int shown=xideEditorShow(app,d); if(shown!=XUI_OK) app->frameError=shown;
            else { xuiCodeEditEnsureCaretVisible(editor); xgeRenderRequest(); }
            return;
        }
    }
}
static void activated(xui_widget widget,int pane,int previous,int current,void* user)
{
    (void)widget; (void)previous; xide_app* app=user;
    /* Dock notifications are deferred. Ignore an event superseded by a later activation. */
    if(xuiDockPanelGetPaneActiveWindow(app->dock,pane)!=current) return;
    xide_document* document=byWindow(app,current);
    if(document) app->activeDocument=document;
    else if(current==app->windows[XIDE_C_DEMO] || current==app->windows[XIDE_XLANG_DEMO]) app->activeDocument=NULL;
    xideActionsRefresh(app);
}
static int dockedPane(xide_app* app,int window)
{
    xui_dock_window_info_t info={0}; info.iSize=sizeof(info);
    return xuiDockPanelGetWindowInfo(app->dock,window,&info)==XUI_OK && info.iState==XUI_DOCK_PANEL_WINDOW_DOCKED ? info.iPane : -1;
}
static int applyTheme(xide_app* app,xui_widget editor)
{
    int styles[]={XUI_CODE_STYLE_DEFAULT,XUI_CODE_STYLE_KEYWORD,XUI_CODE_STYLE_TYPE,XUI_CODE_STYLE_NUMBER,XUI_CODE_STYLE_STRING,XUI_CODE_STYLE_CHAR,XUI_CODE_STYLE_COMMENT};
    ide_color_role roles[]={IDE_TEXT,IDE_KEYWORD,IDE_TYPE,IDE_NUMBER,IDE_STRING,IDE_STRING,IDE_COMMENT};
    for(size_t i=0;i<sizeof(styles)/sizeof(*styles);++i) {
        xui_code_style_t style={sizeof(style),ideThemeColor(app->theme,roles[i]),ideThemeColor(app->theme,IDE_FIELD),0};
        TRY(xuiCodeEditSetStyle(editor,styles[i],&style));
    }
    return XUI_OK;
}
int xideEditorApplyTheme(xide_app* app)
{
    for(size_t i=0;i<app->documents.count;++i) TRY(applyTheme(app,app->documents.items[i]->editor));
    return XUI_OK;
}
int xideEditorShow(xide_app* app,xide_document* document)
{
    if(!document || !document->open) return XUI_ERROR_INVALID_ARGUMENT;
    TRY(xideCatalogBindEditor(app,document->editor)); TRY(xideActionsBindEditor(app,document->editor)); TRY(xideLanguageViewBindEditor(app,document->editor)); TRY(xideCompletionViewBindEditor(app,document->editor)); TRY(xideCCompletionViewBindEditor(app,document->editor)); TRY(xideCLanguageViewBindEditor(app,document->editor));
    if(document->window<0) {
        TRY(xuiDockPanelAddWindow(app->dock,document->title,document->editor,&document->window));
        TRY(xuiDockPanelSetWindowFlags(app->dock,document->window,1,1));
    }
    xui_dock_window_info_t info={0}; info.iSize=sizeof(info);
    TRY(xuiDockPanelGetWindowInfo(app->dock,document->window,&info));
    if(info.iState==XUI_DOCK_PANEL_WINDOW_FLOATING) {
        app->activeDocument=document; TRY(xuiLayout(app->context)); TRY(xuiSetFocusWidget(app->context,document->editor)); documentChanged(document,app); return XUI_OK;
    }
    int pane=dockedPane(app,document->window);
    if(pane<0) {
        for(size_t i=0;i<app->documents.count && pane<0;++i) if(app->documents.items[i]->open) pane=dockedPane(app,app->documents.items[i]->window);
        if(pane<0) pane=dockedPane(app,app->windows[XIDE_XLANG_DEMO]);
        if(pane<0) pane=dockedPane(app,app->windows[XIDE_C_DEMO]);
        if(pane>=0) TRY(xuiDockPanelDockWindowToPane(app->dock,document->window,pane));
        else {
            int bottom=dockedPane(app,app->windows[XIDE_HELP]);
            if(bottom>=0) TRY(xuiDockPanelDockWindowToPaneSide(app->dock,document->window,bottom,XUI_DOCK_PANEL_SIDE_TOP,0.72f,&pane));
            else TRY(xuiDockPanelDockWindow(app->dock,document->window,XUI_DOCK_PANEL_REGION_DOCUMENT,XUI_DOCK_PANEL_SIDE_FILL,0,&pane));
        }
    }
    app->documentPane=pane; TRY(xuiDockPanelSetPaneActiveWindow(app->dock,pane,document->window));
    TRY(xuiDockPanelHideWindow(app->dock,app->windows[XIDE_XLANG_DEMO])); TRY(xuiDockPanelHideWindow(app->dock,app->windows[XIDE_C_DEMO]));
    app->activeDocument=document; TRY(applyTheme(app,document->editor)); TRY(xuiLayout(app->context)); TRY(xuiSetFocusWidget(app->context,document->editor));
    documentChanged(document,app); return XUI_OK;
}
int xideEditorOpen(xide_app* app,const char* path,xide_model_error* error)
{
    xide_document* document=xideDocumentOpen(&app->documents,path,error);
    if(!document) return 0;
    if(xideEditorShow(app,document)!=XUI_OK) {
        if(error) { error->code=XIDE_MODEL_INVALID; snprintf(error->message,sizeof(error->message),"Unable to show document."); }
        return 0;
    }
    return 1;
}
int xideEditorClose(xide_app* app,xide_document* document,int discardChanges,xide_model_error* error)
{
    if(!document || !document->open) return 1;
    int window=document->window;
    if(!xideDocumentClose(document,discardChanges,error)) return 0;
    xide_document* next=NULL;
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open) { next=app->documents.items[i]; break; }
    if(app->activeDocument==document) app->activeDocument=next;
    if(next) xideEditorShow(app,next);
    else xideDispatchAction(app,XIDE_ACTION_XLANG);
    if(window>=0) xuiDockPanelHideWindow(app->dock,window);
    xideActionsRefresh(app); return 1;
}
static int confirmDocument(xide_app* app,xide_document* document)
{
    if(!document->dirty) return 1;
    size_t length=strlen(document->title)+100; char* text=malloc(length);
    if(!text) return 0;
    snprintf(text,length,"文件“%s”有未保存修改。是否保存？",document->title);
    int answer=xideMessageConfirm(app,"保存文件",text,XUI_MSGBOX_BUTTON_YES_NO_CANCEL,xideMessageDocumentKey(document,2,0));free(text);
    return answer==XUI_MSGBOX_RESULT_NO || (answer==XUI_MSGBOX_RESULT_YES && saveDocument(app,document,0));
}
int xideEditorConfirmAll(xide_app* app)
{
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open && !confirmDocument(app,app->documents.items[i])) return 0;
    return 1;
}
static void closeRequested(xui_widget widget,int window,void* user)
{
    (void)widget; xide_app* app=user; xide_document* document=byWindow(app,window);
    if(!document) { xuiDockPanelHideWindow(app->dock,window); return; }
    if(xideActionsHasModal(app)) { app->reopenDocument=document;return; }
    xideMessageBeginClose(app,document);
    if(!confirmDocument(app,document)) { app->reopenDocument=document; xideMessageEndOperation(app);return; }
    xide_model_error error={0};
    if(!xideEditorClose(app,document,1,&error)) { app->reopenDocument=document; report(app,error.message); }
    xideMessageEndOperation(app);
}
int xideEditorPreparePathChange(xide_app* app,const char* source,const char* target,xide_model_error* error)
{
    xideFileRebindDestroy(app->fileRebind); app->fileRebind=NULL; xrtValueRelease(app->fileRebindReading); app->fileRebindReading=NULL;
    if(!xideFileRebindPrepare(&app->workspaceModel,source,target,&app->fileRebind,error)) return 0;
    if(app->fileRebind) {
        if(xideWorkspaceViewHasDraft(app) || xideActionsHasModal(app)) { if(error) { error->code=XIDE_MODEL_CONFLICT; snprintf(error->message,sizeof(error->message),"请先应用或取消属性草稿，并关闭设置窗口，再修改工程文件路径。"); } return 0; }
        for(size_t i=0;i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(!d->open || !d->dirty || !d->path) continue;
            int conflict=app->workspaceModel.manifest && xidePathEqual(d->path,app->workspaceModel.manifest);
            for(size_t j=0;j<xideFileRebindProjectCount(app->fileRebind) && !conflict;++j) conflict=xidePathEqual(d->path,xideFileRebindProjectAt(app->fileRebind,j)->manifest);
            if(conflict) { if(error) { error->code=XIDE_MODEL_CONFLICT; snprintf(error->message,sizeof(error->message),"受影响的工程清单存在未保存正文，请先保存或重载清单。"); } return 0; }
        }
        app->fileRebindReading=xideReadingCapture(app,NULL);
        if(!app->fileRebindReading) { if(error) { error->code=XIDE_MODEL_MEMORY; snprintf(error->message,sizeof(error->message),"无法准备工程阅读状态。"); } return 0; }
    }
    return 1;
}
static int beforeSave(xide_document* document,const char* target,xide_model_error* error,void* user)
{
    xide_app* app=user; int blocked=0;
    xideFileRebindDestroy(app->fileRebind); app->fileRebind=NULL; xrtValueRelease(app->fileRebindReading); app->fileRebindReading=NULL;
    if(app->workspaceModel.manifest && xidePathEqual(target,app->workspaceModel.manifest)) blocked=app->workspaceModel.dirty;
    for(size_t i=0;i<app->workspaceModel.projectCount;++i) {
        xide_project* project=&app->workspaceModel.projects[i];
        if(project->dirty && xidePathEqual(target,project->manifest)) blocked=1;
    }
    if(blocked && error) { error->code=XIDE_MODEL_CONFLICT; snprintf(error->message,sizeof(error->message),"Manifest properties have unsaved changes. Save the workspace/project before editing its manifest text."); }
    if(blocked || !xideFileSyncBeforeSave(document,target,error,user)) return 0;
    return xideEditorPreparePathChange(app,document->path,target,error);
}
static void pathChanged(xide_document* document,const char* previous,const char* target,void* user)
{ (void)document; xide_app* app=user; xideTasksInvalidateInput(app->tasks,previous); xideTasksInvalidateInput(app->tasks,target); }
void xideEditorFinishPathChange(xide_app* app,const char* target,int saved)
{
    xide_file_rebind* plan=app->fileRebind;
    if(saved && plan) {
        for(size_t i=0;i<xideFileRebindProjectCount(plan);++i) xideTasksInvalidateInput(app->tasks,xideFileRebindProjectAt(plan,i)->manifest);
        xideFileRebindApply(plan); xideTasksInvalidateInput(app->tasks,target);
        int refreshed=xideWorkspaceViewRefresh(app); if(refreshed!=XUI_OK) app->frameError=refreshed;
        if(refreshed==XUI_OK) { xideReadingRestore(app,app->fileRebindReading,0); xideWorkbenchLayout(app); int laid=xuiLayout(app->context); if(laid==XUI_OK) xideReadingRestore(app,app->fileRebindReading,1); else app->frameError=laid; }
    }
    xideFileRebindDestroy(plan); app->fileRebind=NULL; xrtValueRelease(app->fileRebindReading); app->fileRebindReading=NULL;
}
static void finishSave(xide_document* document,int saved,void* user)
{ xideEditorFinishPathChange(user,document->path,saved); xideLanguageViewSaved(user,document,saved); }
int xideEditorViewInit(xide_app* app)
{
    app->editorPosition.valid=0;
    xideDocumentsInit(&app->documents,app->context,app->codeFont,documentChanged,app);
    TRY(xuiCodeFindScopeCreate(&app->editorFindScope));
    TRY(xuiCodeFindScopeSetActivate(app->editorFindScope,findActivated,app));
    app->documents.beforeSave=beforeSave; app->documents.afterSave=xideFileSyncAfterSave; app->documents.saveFinished=finishSave; app->documents.pathChanged=pathChanged;
    TRY(xuiDockPanelSetWindowClose(app->dock,closeRequested,app)); TRY(xuiDockPanelSetActiveChanged(app->dock,activated,app));
    xideActionsRefresh(app); return XUI_OK;
}
int xideEditorReset(xide_app* app)
{
    xide_document* active=app->activeDocument; int any=0;
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open) {
        xide_document* document=app->documents.items[i];
        TRY(xuiDockPanelDockWindowToPane(app->dock,document->window,app->documentPane)); any=1;
    }
    if(any) {
        TRY(xuiDockPanelHideWindow(app->dock,app->windows[XIDE_XLANG_DEMO])); TRY(xuiDockPanelHideWindow(app->dock,app->windows[XIDE_C_DEMO]));
        if(!active || !active->open) for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open) { active=app->documents.items[i]; break; }
        app->activeDocument=active; TRY(xuiDockPanelSetPaneActiveWindow(app->dock,app->documentPane,active->window));
    }
    xideActionsRefresh(app); return XUI_OK;
}
static void statusPosition(xide_app* app,xide_document* document,int* line,int* column)
{
    xide_editor_position* position=&app->editorPosition;
    xui_code_document core=xuiCodeEditGetDocument(document->editor);
    xui_code_selection_t selection={0};
    *line=*column=0;
    if(!core || xuiCodeSelectionGetState(xuiCodeEditGetSelection(document->editor),&selection)!=XUI_OK) {
        position->valid=0; return;
    }
    uint32_t version=xuiCodeDocumentGetVersion(core);
    if(position->valid && position->owner==document && position->document==core &&
       position->documentId==document->id && position->version==version && position->offset==selection.iCaretOffset) {
        *line=position->line; *column=position->column; return;
    }
    position->valid=0;
    if(xuiCodeDocumentOffsetToLineColumn(core,selection.iCaretOffset,line,column)!=XUI_OK) {
        *line=*column=0; return;
    }
    position->owner=document; position->document=core; position->documentId=document->id;
    position->version=version; position->offset=selection.iCaretOffset;
    position->line=*line; position->column=*column; position->valid=1;
}
void xideEditorPoll(xide_app* app)
{
    xideCatalogViewPoll(app); xideCompletionViewPoll(app); xideCCompletionViewPoll(app);
    if(app->reopenDocument) { xide_document* document=app->reopenDocument; app->reopenDocument=NULL; xideEditorShow(app,document); }
    xide_document* document=app->activeDocument; char* status=app->editorStatus;
    if(document && document->open) {
        int line=0,column=0;
        statusPosition(app,document,&line,&column);
        const char* eol=document->baseline.eol==XUI_CODE_EOL_CRLF ? "CRLF" : document->baseline.eol==XUI_CODE_EOL_CR ? "CR" : "LF";
        snprintf(status,sizeof(app->editorStatus),"%s · UTF-8%s · %s · %d:%d",document->language,document->baseline.bom ? " BOM" : "",eol,line+1,column+1);
        xuiStatusBarSetItemText(app->statusBar,0,document->dirty ? "文件有未保存修改" : "就绪");
    } else {
        app->editorPosition.valid=0;
        snprintf(status,sizeof(app->editorStatus),"C / xlang · UTF-8"); xuiStatusBarSetItemText(app->statusBar,0,"就绪 · 可打开或新建源文件");
    }
    xuiStatusBarSetItemWidth(app->statusBar,2,270); xuiStatusBarSetItemText(app->statusBar,2,status);
}
int xideEditorAction(xide_app* app,int action)
{
    const xide_action_info* definition=xideActionFind(action);
    if((action<XIDE_ACTION_NEW_FILE || action>XIDE_ACTION_EDITOR_COMPLETE) && (action<XIDE_ACTION_EDITOR_CUT || action>XIDE_ACTION_EDITOR_COMMENT)) return 0;
    xide_model_error error={0}; xide_document* document=app->activeDocument; char* path=NULL; int ok=1;
    if(action==XIDE_ACTION_NEW_FILE) {
        document=xideDocumentNew(&app->documents,"xlang",&error); ok=document && xideEditorShow(app,document)==XUI_OK;
    } else if(action==XIDE_ACTION_OPEN_FILE) {
        path=choosePath(app,0,NULL); if(path) ok=xideEditorOpen(app,path,&error);
    } else if(action==XIDE_ACTION_SAVE_ALL_FILES) {
        for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->open && app->documents.items[i]->dirty && !saveDocument(app,app->documents.items[i],0)) break;
    } else if(document && document->open) {
        if(action==XIDE_ACTION_SAVE_FILE || action==XIDE_ACTION_SAVE_FILE_AS) saveDocument(app,document,action==XIDE_ACTION_SAVE_FILE_AS);
        else if(action==XIDE_ACTION_CLOSE_FILE) { if(confirmDocument(app,document)) ok=xideEditorClose(app,document,1,&error); }
        else if(action==XIDE_ACTION_RELOAD_FILE) {
            if(!document->dirty || xideMessageConfirm(app,"重新加载文件","重新加载将放弃此文件未保存的修改。是否继续？",XUI_MSGBOX_BUTTON_YES_NO,xideMessageDocumentKey(document,3,0))==XUI_MSGBOX_RESULT_YES) ok=xideDocumentReload(document,1,&error);
        } else if(action>=XIDE_ACTION_LANGUAGE_C && action<=XIDE_ACTION_LANGUAGE_TEXT) {
            const char* modes[]={"c","xlang","text"}; ok=xideDocumentSetLanguage(document,modes[action-XIDE_ACTION_LANGUAGE_C],&error);
        } else if(action==XIDE_ACTION_EDITOR_COMPLETE) {
            xuiSetFocusWidget(app->context,document->editor);
            xui_code_selection_t selection={0}; xuiCodeSelectionGetState(xuiCodeEditGetSelection(document->editor),&selection);
            xideCompletionViewRequest(app,document->editor,selection.iCaretOffset,1); xideCCompletionViewRequest(app,document->editor,selection.iCaretOffset,1); xuiCodeEditShowCompletion(document->editor);
        } else {
            xuiSetFocusWidget(app->context,document->editor);
            xui_event_t event={0}; event.iSize=sizeof(event); event.iType=XUI_EVENT_COMMAND; event.pTarget=document->editor; event.iCommand=definition ? definition->codeCommand : 0;
            ok=event.iCommand && xuiDispatchEvent(app->context,&event)>=XUI_OK;
        }
    }
    free(path); if(!ok) report(app,error.message); xideActionsRefresh(app); return 1;
}
