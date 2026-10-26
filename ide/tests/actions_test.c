#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "ui/task_view.h"
#include "ui/configuration_view.h"
#include "services/file_sync.h"
#include "services/ui_font.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct fixture { const char *directory,*compiler; HDESK desktop; int started,failed; } fixture;
#define CHECK(v) do { if(!(v)) { fprintf(stderr,"actions_test:%d: %s; %s\n",__LINE__,#v,error.message); f->failed=1; goto cleanup; } } while(0)
static int writeText(const char* p,const char* text) { return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* a)
{
    if(xuiDispatchPendingEvents(a->context)!=XUI_OK) return 0;
    xideEditorPoll(a); xideFileSyncPoll(a); xideTaskViewPoll(a); xideWorkbenchLayout(a);
    return xuiLayout(a->context)==XUI_OK && !a->frameError;
}
static int key(xide_app* a,int k,uint32_t modifiers)
{ return xuiInputKeyDownEx(a->context,k,modifiers,NULL)==XUI_OK && xuiInputKeyUpEx(a->context,k,0,NULL)==XUI_OK && settle(a); }
static int pointer(xide_app* a,xui_rect_t r)
{ float x=r.fX+r.fW/2,y=r.fY+r.fH/2; return xuiInputPointerMove(a->context,x,y,0)==XUI_OK && xuiInputPointerDown(a->context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a->context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && settle(a); }
static xui_widget find(xui_widget w,xui_widget_type type,const char* text)
{
    if(xuiWidgetGetType(w)==type && (!text || !strcmp(xuiButtonGetText(w),text))) return w;
    for(xui_widget child=xuiWidgetGetFirstChild(w);child;child=xuiWidgetGetNextSibling(child)) { xui_widget result=find(child,type,text); if(result) return result; }
    return NULL;
}
static int allStates(xide_app* a)
{
    for(int n=0;n<xuiMenuBarGetItemCount(a->menuBar);++n) {
        xui_widget menu=xuiMenuBarGetItem(a->menuBar,n)->pMenu;
        for(int i=0;i<xuiMenuGetItemCount(menu);++i) {
            const xui_menu_item_t* item=xuiMenuGetItem(menu,i); if(!item->iValue) continue;
            const xide_action_info* action=xideActionFind(item->iValue); xide_action_state state=xideActionQuery(a,item->iValue);
            if(!action || strcmp(action->title,item->sText) || (action->shortcut && (!item->sShortcut || strcmp(action->shortcut,item->sShortcut))) || !!(item->iState&XUI_MENU_ITEM_ENABLED)!=!!state.enabled || !!(item->iState&XUI_MENU_ITEM_CHECKED)!=!!state.checked) return 0;
        }
    }
    for(int i=0;i<xuiToolbarGetItemCount(a->toolbar);++i) { const xui_toolbar_item_t* item=xuiToolbarGetItem(a->toolbar,i); if(item->iValue && !!xuiToolbarIsItemEnabled(a->toolbar,i)!=!!xideActionQuery(a,item->iValue).enabled) return 0; }
    return 1;
}
static int menuAction(xide_app* a,xui_widget menu,int id)
{
    int index=-1; for(int i=0;i<xuiMenuGetItemCount(menu);++i) if(xuiMenuGetItem(menu,i)->iValue==id) index=i;
    if(index<0 || xuiMenuOpenAt(menu,a->root,80,60)!=XUI_OK || !settle(a)) return 0;
    xui_rect_t r=xuiMenuGetItemRect(menu,index),world=xuiWidgetGetWorldRect(menu); r.fX+=world.fX; r.fY+=world.fY;
    if(xuiInputPointerMove(a->context,r.fX+r.fW/2,r.fY+r.fH/2,0)!=XUI_OK || !settle(a) || xuiMenuGetHoverIndex(menu)!=index) return 0;
    return pointer(a,r);
}
static int contextAction(xide_app* a,xide_document* doc,int action,int disabled)
{
    if(xuiCodeEditOpenMenu(doc->editor,380,220)!=XUI_OK || !settle(a)) return 0;
    xui_widget menu=xuiCodeEditGetMenuWidget(doc->editor); int index=-1;
    for(int i=0;i<xuiMenuGetItemCount(menu);++i) if(!strcmp(xuiMenuGetItem(menu,i)->sText ? xuiMenuGetItem(menu,i)->sText : "",xideActionFind(action)->title)) index=i;
    if(index<0 || !!(xuiMenuGetItemState(menu,index)&XUI_MENU_ITEM_ENABLED)==!!disabled) return 0;
    if(disabled) return xuiMenuClose(menu)==XUI_OK && settle(a);
    xui_rect_t r=xuiMenuGetItemRect(menu,index),world=xuiWidgetGetWorldRect(menu); r.fX+=world.fX; r.fY+=world.fY;
    return pointer(a,r);
}
static int captureMenu(xide_app* a,int barIndex,const char* path)
{
    if(xuiMenuBarOpenItem(a->menuBar,barIndex)!=XUI_OK || !settle(a)) return 0;
    int result=xuiUpdate(a->context,0)==XUI_OK; xui_rect_i_t rect={0,0,a->viewportWidth,a->viewportHeight};
    if(result) result=a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK && xuiRender(a->context,a->target,&rect,1)==XUI_OK && xideAppCapture(a,path)==XGE_OK;
    xuiMenuBarClose(a->menuBar); return result && settle(a);
}
static int waitPhase(xide_app* a,xide_task* task,int input)
{
    uint64_t until=GetTickCount64()+15000;
    do { if(!settle(a)) return 0; xide_task_info info={0}; xideTaskInfo(task,&info); if(input ? xideTaskInputOpen(task) : info.state>=XIDE_TASK_SUCCEEDED) return settle(a); if(input && info.state>=XIDE_TASK_SUCCEEDED) { fprintf(stderr,"task before stdin: state=%d phase=%d error=%s\n%s\n",info.state,info.phase,info.error,xideTaskOutput(task)); return 0; } Sleep(5); } while(GetTickCount64()<until);
    return 0;
}
static int command(xide_app* a,xide_document* doc,int id)
{ xui_event_t e={0}; e.iSize=sizeof(e); e.iType=XUI_EVENT_COMMAND; e.pTarget=doc->editor; e.iCommand=id; return xuiDispatchEvent(a->context,&e)>=XUI_OK && settle(a); }
/* The real SDK executes clipboard commands through this private text provider.
   Do not read or replace the developer's Windows clipboard during verification. */
static char commandClipboard[4096];
static int commandClipboardSet(xui_proxy proxy,const char* text)
{ (void)proxy; if(!text || strlen(text)>=sizeof(commandClipboard)) return XUI_ERROR_INVALID_ARGUMENT; strcpy(commandClipboard,text); return XUI_OK; }
static int commandClipboardGet(xui_proxy proxy,char* text,int capacity)
{ (void)proxy; size_t length=strlen(commandClipboard); if(text && capacity>0) { size_t copied=length<(size_t)capacity-1 ? length : (size_t)capacity-1; memcpy(text,commandClipboard,copied); text[copied]=0; } return (int)length; }
static int commandPrivateSetProxy(xui_context context,const xui_proxy_t* original)
{ xui_proxy_t proxy=*original; proxy.clipboardSetText=commandClipboardSet; proxy.clipboardGetText=commandClipboardGet; return xuiSetProxy(context,&proxy); }
#define xuiSetProxy commandPrivateSetProxy
#include "app/xide_app.c"
#undef xuiSetProxy
static int editorCommands(xide_app* a)
{
    int passed=0,checks=0; xide_model_error error={0};
    xui_proxy_t original={0}; xide_document *saved=a->activeDocument,*doc=NULL,*other=NULL;
#define COMMAND_CHECK(v) do { ++checks; if(!(v)) { fprintf(stderr,"editor commands:%d: %s; own-active=%d own-focus=%d dirty=%d text=%.120s\n",__LINE__,#v,doc && a->activeDocument==doc,doc && xuiGetFocusWidget(a->context)==doc->editor,doc ? doc->dirty : -1,doc ? xuiCodeEditGetText(doc->editor) : "<no-document>"); goto commands_cleanup; } } while(0)
    COMMAND_CHECK(xuiGetProxy(a->context,&original)==XUI_OK && original.clipboardSetText==commandClipboardSet && original.clipboardGetText==commandClipboardGet);
    commandClipboard[0]=0;
    doc=xideDocumentNew(&a->documents,"c",&error); other=xideDocumentNew(&a->documents,"xlang",&error);
    COMMAND_CHECK(doc && other && xideEditorShow(a,doc)==XUI_OK && settle(a));
    xui_code_document core=xuiCodeEditGetDocument(doc->editor),otherCore=xuiCodeEditGetDocument(other->editor);
    xui_code_selection_model selection=xuiCodeEditGetSelection(doc->editor);
    COMMAND_CHECK(xuiCodeDocumentSetText(otherCore,"另一文档😀") == XUI_OK);
    COMMAND_CHECK(!xideActionQuery(a,XIDE_ACTION_EDITOR_COPY).enabled && !xideActionQuery(a,XIDE_ACTION_EDITOR_CUT).enabled && !xideActionQuery(a,XIDE_ACTION_EDITOR_SELECT_ALL).enabled);
    COMMAND_CHECK(xuiCodeDocumentSetText(core,"中文😀 alpha\nsecond\n")==XUI_OK && settle(a));
    int length=xuiCodeDocumentGetLength(core);
    COMMAND_CHECK(xuiCodeSelectionSetRange(selection,core,(int)strlen("中文😀"),0)==XUI_OK && settle(a));
    uint32_t version=xuiCodeDocumentGetVersion(core); xui_code_selection_t before={0},after={0};
    COMMAND_CHECK(xuiCodeSelectionGetState(selection,&before)==XUI_OK && xideActionQuery(a,XIDE_ACTION_EDITOR_COPY).enabled && xideActionQuery(a,XIDE_ACTION_EDITOR_CUT).enabled);
    COMMAND_CHECK(menuAction(a,a->editMenu,XIDE_ACTION_EDITOR_COPY) && !strcmp(commandClipboard,"中文😀"));
    COMMAND_CHECK(xuiCodeDocumentGetVersion(core)==version && xuiCodeSelectionGetState(selection,&after)==XUI_OK && before.iAnchorOffset==after.iAnchorOffset && before.iCaretOffset==after.iCaretOffset);
    COMMAND_CHECK(key(a,'A',XUI_MOD_CTRL) && xuiCodeSelectionGetState(selection,&after)==XUI_OK && after.iAnchorOffset==0 && after.iCaretOffset==length);
    COMMAND_CHECK(contextAction(a,doc,XIDE_ACTION_EDITOR_COPY,0) && !strcmp(commandClipboard,"中文😀 alpha\nsecond\n"));
    COMMAND_CHECK(key(a,'X',XUI_MOD_CTRL));
    COMMAND_CHECK(!*xuiCodeDocumentGetText(core) && !doc->dirty);
    COMMAND_CHECK(key(a,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),"中文😀 alpha\nsecond\n") && doc->dirty);
    COMMAND_CHECK(key(a,'Y',XUI_MOD_CTRL) && !*xuiCodeDocumentGetText(core));
    COMMAND_CHECK(key(a,'V',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),commandClipboard));
    COMMAND_CHECK(key(a,'Z',XUI_MOD_CTRL) && !*xuiCodeDocumentGetText(core));
    COMMAND_CHECK(key(a,'Y',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),commandClipboard));
    COMMAND_CHECK(xuiCodeSelectionSetRange(selection,core,0,(int)strlen("中文😀"))==XUI_OK && xuiCodeEditSetReadonly(doc->editor,1)==XUI_OK && settle(a));
    version=xuiCodeDocumentGetVersion(core);
    const int mutating[]={XIDE_ACTION_EDITOR_CUT,XIDE_ACTION_EDITOR_PASTE,XIDE_ACTION_EDITOR_INDENT,XIDE_ACTION_EDITOR_OUTDENT,XIDE_ACTION_EDITOR_COMMENT};
    for(size_t i=0;i<sizeof(mutating)/sizeof(*mutating);++i) { COMMAND_CHECK(!xideActionQuery(a,mutating[i]).enabled && xideActionExecute(a,mutating[i])==XIDE_ACTION_DISABLED); }
    COMMAND_CHECK(key(a,'X',XUI_MOD_CTRL) && key(a,'V',XUI_MOD_CTRL) && key(a,']',XUI_MOD_CTRL) && key(a,'[',XUI_MOD_CTRL) && key(a,'/',XUI_MOD_CTRL));
    COMMAND_CHECK(xuiCodeDocumentGetVersion(core)==version && contextAction(a,doc,XIDE_ACTION_EDITOR_CUT,1) && contextAction(a,doc,XIDE_ACTION_EDITOR_PASTE,1) && contextAction(a,doc,XIDE_ACTION_EDITOR_COMMENT,1));
    COMMAND_CHECK(key(a,'C',XUI_MOD_CTRL) && !strcmp(commandClipboard,"中文😀") && key(a,'A',XUI_MOD_CTRL) && xuiCodeDocumentGetVersion(core)==version);
    COMMAND_CHECK(xuiCodeEditSetReadonly(doc->editor,0)==XUI_OK && settle(a));
    const char* lines="    alpha😀\n\tbeta\nlast";
    COMMAND_CHECK(xuiCodeDocumentSetText(core,lines)==XUI_OK && xuiCodeEditSetIndentColumns(doc->editor,4)==XUI_OK && xuiCodeEditSetExpandTabs(doc->editor,1)==XUI_OK && settle(a));
    COMMAND_CHECK(xuiCodeSelectionSetRange(selection,core,(int)strlen("    alpha😀\n\tbeta\n"),0)==XUI_OK && settle(a));
    COMMAND_CHECK(key(a,']',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),"        alpha😀\n    \tbeta\nlast"));
    COMMAND_CHECK(key(a,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),lines));
    COMMAND_CHECK(key(a,'Y',XUI_MOD_CTRL) && key(a,'[',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),lines));
    COMMAND_CHECK(xuiCodeSelectionSetRange(selection,core,(int)strlen("    alpha😀\n\tbeta\n"),0)==XUI_OK && settle(a));
    COMMAND_CHECK(menuAction(a,a->editMenu,XIDE_ACTION_EDITOR_COMMENT) && !strcmp(xuiCodeDocumentGetText(core),"    //alpha😀\n\t//beta\nlast"));
    COMMAND_CHECK(key(a,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),lines));
    COMMAND_CHECK(key(a,'Y',XUI_MOD_CTRL) && key(a,'/',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),lines));
    COMMAND_CHECK(xideDocumentSetLanguage(doc,"xlang",&error) && settle(a) && xideActionQuery(a,XIDE_ACTION_EDITOR_COMMENT).enabled);
    COMMAND_CHECK(xuiCodeDocumentSetText(core,"func main(...) as int\n return 0\nend\n")==XUI_OK && xuiCodeSelectionSetRange(selection,core,0,(int)strlen("func main(...) as int\n"))==XUI_OK && settle(a));
    COMMAND_CHECK(contextAction(a,doc,XIDE_ACTION_EDITOR_COMMENT,0) && !strcmp(xuiCodeDocumentGetText(core),"//func main(...) as int\n return 0\nend\n"));
    COMMAND_CHECK(key(a,'/',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),"func main(...) as int\n return 0\nend\n"));
    COMMAND_CHECK(xideDocumentSetLanguage(doc,"text",&error) && settle(a) && !xideActionQuery(a,XIDE_ACTION_EDITOR_COMMENT).enabled);
    version=xuiCodeDocumentGetVersion(core);
    COMMAND_CHECK(key(a,'/',XUI_MOD_CTRL) && command(a,doc,XUI_CODE_COMMAND_TOGGLE_LINE_COMMENT) && contextAction(a,doc,XIDE_ACTION_EDITOR_COMMENT,1) && xuiCodeDocumentGetVersion(core)==version);
    COMMAND_CHECK(menuAction(a,a->editMenu,XIDE_ACTION_EDITOR_SELECT_ALL) && menuAction(a,a->editMenu,XIDE_ACTION_EDITOR_INDENT) && key(a,'Z',XUI_MOD_CTRL));
    COMMAND_CHECK(xideDocumentSetLanguage(doc,"c",&error) && settle(a) && xideActionQuery(a,XIDE_ACTION_EDITOR_COMMENT).enabled);
    COMMAND_CHECK(xuiCodeSelectionSetRange(selection,core,0,(int)strlen("func"))==XUI_OK && xuiSetFocusWidget(a->context,doc->editor)==XUI_OK);
    a->activeDocument=other;
    COMMAND_CHECK(key(a,'C',XUI_MOD_CTRL) && a->activeDocument==doc && !strcmp(commandClipboard,"func") && !strcmp(xuiCodeDocumentGetText(otherCore),"另一文档😀"));
    xui_widget field=find(a->properties,xuiWidgetFindType(a->context,"input"),NULL);
    COMMAND_CHECK(field && xuiEditSetText(field,"字段😀") == XUI_OK && xuiSetFocusWidget(a->context,field)==XUI_OK && settle(a));
    version=xuiCodeDocumentGetVersion(core);
    COMMAND_CHECK(key(a,'A',XUI_MOD_CTRL) && key(a,'C',XUI_MOD_CTRL) && !strcmp(commandClipboard,"字段😀") && xuiCodeDocumentGetVersion(core)==version);
    COMMAND_CHECK(xuiSetFocusWidget(a->context,doc->editor)==XUI_OK && key(a,XUI_KEY_TAB,0) && key(a,'Z',XUI_MOD_CTRL) && settle(a));
    COMMAND_CHECK(allStates(a));
    printf("editor_commands passed: %d native checks; seven registered edit commands, controlled private clipboard, Unicode/reverse selection and one-step undo, C/xlang/text comment modes, read-only guards, native menu/context/keys and plain-field isolation\n",checks);
    passed=1;
commands_cleanup:
    if(doc && doc->open) xideEditorClose(a,doc,1,&error);
    if(other && other->open) xideEditorClose(a,other,1,&error);
    if(saved && saved->open) xideEditorShow(a,saved);
    commandClipboard[0]=0;
#undef COMMAND_CHECK
    return passed && settle(a);
}
static int verify(void* user)
{
    fixture* f=user; if(f->started) return XGE_OK; f->started=1;
    xide_app app={0}; xide_model_error error={0}; xide_workspace ws={0}; int began=0;
    char *root=NULL,*entry=NULL,*cPath=NULL,*alpha=NULL,*beta=NULL,*workspace=NULL,*settings=NULL,*emptyImage=NULL,*editImage=NULL,*languageImage=NULL;
    char* id=xideCreateId("actions"); root=id ? xidePathResolve(f->directory,id) : NULL; free(id); CHECK(root && xrtDirCreateAll(root));
    entry=xidePathResolve(root,"main.xl"); cPath=xidePathResolve(root,"helper.c"); alpha=xidePathResolve(root,"alpha.xlp"); beta=xidePathResolve(root,"beta.xlp"); workspace=xidePathResolve(root,"workspace.xlw"); settings=xidePathResolve(root,"settings.json");
    emptyImage=xidePathResolve(root,"native-actions-empty.png"); editImage=xidePathResolve(root,"native-actions-c.png"); languageImage=xidePathResolve(root,"native-actions-xlang.png"); CHECK(entry && cPath && alpha && beta && workspace && settings && emptyImage && editImage && languageImage);
    CHECK(writeText(entry,"FAKE_OK\n") && writeText(cPath,"int helper = 1; // 中文😀\n"));
    CHECK(writeText(alpha,"{\"format\":\"xlang-project-1\",\"kind\":\"exe\",\"entry\":\"main.xl\",\"ide\":{\"format\":\"xlang-ide-project-1\",\"name\":\"Alpha 启动项目\",\"configurations\":[{\"id\":\"dev\",\"name\":\"开发\",\"output\":\"build/alpha.exe\",\"run\":{\"workingDirectory\":\".\",\"environment\":{\"XIDE_TASK_MODE\":\"echo\"}}}]}}"));
    CHECK(writeText(beta,"{\"format\":\"xlang-project-1\",\"kind\":\"dll\",\"entry\":\"main.xl\",\"ide\":{\"format\":\"xlang-ide-project-1\",\"name\":\"Beta 所选项目\",\"configurations\":[{\"id\":\"dev\",\"name\":\"开发\",\"output\":\"build/beta.dll\"}]}}"));
    CHECK(xideWorkspaceCreate(&ws,"动作验收",&error) && xideWorkspaceAddProject(&ws,alpha,"alpha",&error) && xideWorkspaceAddProject(&ws,beta,"beta",&error) && xideWorkspaceSetStartup(&ws,"alpha",&error) && xideWorkspaceSave(&ws,workspace,&error));
    app.hidden=1; app.noSession=1; app.highDpi=1; app.dpiOverride=1.25f; app.settingsPath=settings; app.compilerPath=f->compiler;
    CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK); began=1;
    CHECK(settle(&app) && allStates(&app) && xideActionCount()==75 && !xideActionAt(xideActionCount()) && !xideActionNamed(NULL) && !xideActionNamed("missing.action") && xideActionExecute(&app,999)==XIDE_ACTION_UNKNOWN);
    for(size_t i=0;i<xideActionCount();++i) { const xide_action_info* a=xideActionAt(i); CHECK(a && a->name && a->title && xideActionNamed(a->name)==a && xideActionFind(a->id)==a); for(size_t j=i+1;j<xideActionCount();++j) { const xide_action_info* b=xideActionAt(j); CHECK(a->id!=b->id && strcmp(a->name,b->name) && (!a->key || a->key!=b->key || a->modifiers!=b->modifiers || (a->shortcutScope!=b->shortcutScope && a->shortcutScope!=XIDE_SHORTCUT_GLOBAL && b->shortcutScope!=XIDE_SHORTCUT_GLOBAL))); } }
    CHECK(!xideActionQuery(&app,XIDE_ACTION_SAVE_FILE).enabled && !xideActionQuery(&app,XIDE_ACTION_STOP).enabled && xideActionExecute(&app,XIDE_ACTION_RELOAD_FILE)==XIDE_ACTION_DISABLED && captureMenu(&app,0,emptyImage));
    CHECK(key(&app,'N',XUI_MOD_CTRL) && app.activeDocument && !app.activeDocument->path && !xideActionQuery(&app,XIDE_ACTION_RELOAD_FILE).enabled && xideActionQuery(&app,XIDE_ACTION_SAVE_FILE).enabled && !xideActionQuery(&app,XIDE_ACTION_EDITOR_UNDO).enabled);
    xide_document* unnamed=app.activeDocument; CHECK(xideEditorClose(&app,unnamed,1,&error) && xideEditorOpen(&app,cPath,&error) && settle(&app));
    xide_document* cd=app.activeDocument; xui_code_document core=xuiCodeEditGetDocument(cd->editor); uint32_t version=xuiCodeDocumentGetVersion(core);
    CHECK(xuiGetFocusWidget(app.context)==cd->editor);
    CHECK(xuiInputText(app.context,'X')==XUI_OK && settle(&app));
    CHECK(cd->dirty && xideActionQuery(&app,XIDE_ACTION_EDITOR_UNDO).enabled && !xideActionQuery(&app,XIDE_ACTION_EDITOR_REDO).enabled);
    CHECK(allStates(&app));
    CHECK(captureMenu(&app,2,editImage));
    CHECK(menuAction(&app,app.editMenu,XIDE_ACTION_EDITOR_UNDO) && !cd->dirty && xideActionQuery(&app,XIDE_ACTION_EDITOR_REDO).enabled);
    CHECK(key(&app,'Y',XUI_MOD_CTRL) && cd->dirty && xuiCodeDocumentGetVersion(core)>version);
    CHECK(command(&app,cd,XUI_CODE_COMMAND_UNDO) && !cd->dirty && command(&app,cd,XUI_CODE_COMMAND_REDO) && cd->dirty);
    CHECK(xuiCodeEditSetReadonly(cd->editor,1)==XUI_OK && settle(&app) && !xideActionQuery(&app,XIDE_ACTION_EDITOR_UNDO).enabled && !xideActionQuery(&app,XIDE_ACTION_EDITOR_REPLACE).enabled);
    version=xuiCodeDocumentGetVersion(core); CHECK(key(&app,'Z',XUI_MOD_CTRL));
    CHECK(xuiCodeDocumentGetVersion(core)==version);
    CHECK(command(&app,cd,XUI_CODE_COMMAND_UNDO));
    CHECK(xuiCodeDocumentGetVersion(core)==version && contextAction(&app,cd,XIDE_ACTION_EDITOR_UNDO,1) && xuiCodeEditSetReadonly(cd->editor,0)==XUI_OK);
    CHECK(contextAction(&app,cd,XIDE_ACTION_EDITOR_UNDO,0) && !cd->dirty && key(&app,'Y',XUI_MOD_CTRL) && cd->dirty);
    CHECK(contextAction(&app,cd,XIDE_ACTION_EDITOR_FIND,0) && xuiCodeEditGetFindWindow(cd->editor) && xuiWidgetGetVisible(xuiCodeEditGetFindWindow(cd->editor)) && key(&app,XUI_KEY_ESCAPE,0) && !xuiWidgetGetVisible(xuiCodeEditGetFindWindow(cd->editor)));
    xui_widget nativeMenu=xuiCodeEditGetMenuWidget(cd->editor); CHECK(xuiWidgetGetType(cd->editor)==xuiWidgetFindType(app.context,"codeedit") && xuiMenuGetFont(nativeMenu)==app.uiFont);
    CHECK(xideUiFontApply(&app,app.uiFontPath,24,&error) && settle(&app) && xuiMenuGetFont(nativeMenu)==app.uiFont && xideUiFontApply(&app,app.uiFontPath,16,&error) && settle(&app) && xuiMenuGetFont(nativeMenu)==app.uiFont);
    wchar_t lockedPath[32768]; CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,settings,-1,lockedPath,32768));
    HANDLE lock=CreateFileW(lockedPath,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL); CHECK(lock!=INVALID_HANDLE_VALUE);
    xui_font previousFont=app.uiFont; int blocked=!xideUiFontApply(&app,app.uiFontPath,20,&error); CloseHandle(lock);
    CHECK(blocked && app.uiFont==previousFont && app.uiFontSize==16 && xuiMenuGetFont(nativeMenu)==previousFont && settle(&app));
    CHECK(xideEditorShow(&app,cd)==XUI_OK && settle(&app));
    xui_rect_t saveRect=xuiToolbarGetItemRect(app.toolbar,1),toolbarWorld=xuiWidgetGetWorldRect(app.toolbar); saveRect.fX+=toolbarWorld.fX; saveRect.fY+=toolbarWorld.fY;
    CHECK(pointer(&app,saveRect) && !cd->dirty);

    CHECK(xideEditorOpen(&app,entry,&error) && settle(&app)); xide_document* xd=app.activeDocument;
    CHECK(xideActionQuery(&app,XIDE_ACTION_LANGUAGE_XLANG).checked);
    CHECK(menuAction(&app,app.languageMenu,XIDE_ACTION_LANGUAGE_C));
    CHECK(!strcmp(xd->language,"c") && xideActionQuery(&app,XIDE_ACTION_LANGUAGE_C).checked && !xideActionQuery(&app,XIDE_ACTION_LANGUAGE_XLANG).checked);
    CHECK(menuAction(&app,app.languageMenu,XIDE_ACTION_LANGUAGE_XLANG) && xideActionExecute(&app,XIDE_ACTION_THEME_GRAPHITE)==XIDE_ACTION_DISPATCHED && settle(&app) && xideActionQuery(&app,XIDE_ACTION_THEME_GRAPHITE).checked && !xideActionQuery(&app,XIDE_ACTION_THEME_CLASSIC).checked && allStates(&app) && captureMenu(&app,4,languageImage));
    CHECK(xideWorkspaceOpen(&app,workspace,0,&error) && xideWorkspaceViewSelectProject(&app,"beta") && settle(&app) && xideActionQuery(&app,XIDE_ACTION_BUILD).enabled && xideActionQuery(&app,XIDE_ACTION_RUN).enabled && !xideActionQuery(&app,XIDE_ACTION_SET_STARTUP).checked && allStates(&app));
    xui_widget reordered=NULL; xui_menu_desc_t md={0}; md.iSize=sizeof(md); md.pFont=app.uiFont; md.pOwner=app.root; const int order[]={XIDE_ACTION_RELOAD_FILE,XIDE_ACTION_RUN,0,XIDE_ACTION_SAVE_FILE,XIDE_ACTION_THEME_GRAPHITE};
    CHECK(xuiMenuCreate(app.context,&reordered,&md)==XUI_OK && xideActionsMenu(&app,reordered,order,5)==XUI_OK);
    CHECK(xideWorkspaceSetStartup(&app.workspaceModel,"beta",&error) && settle(&app) && !xideActionQuery(&app,XIDE_ACTION_RUN).enabled && !(xuiMenuGetItemState(reordered,1)&XUI_MENU_ITEM_ENABLED));
    size_t tasks=xideTasksCount(app.tasks); CHECK(xuiMenuSetItemState(app.fileMenu,7,XUI_MENU_ITEM_ENABLED,XUI_MENU_ITEM_ENABLED)==XUI_OK && xideActionExecute(&app,XIDE_ACTION_RUN)==XIDE_ACTION_DISABLED && key(&app,XUI_KEY_F5,0) && xideTasksCount(app.tasks)==tasks && allStates(&app));
    CHECK(xideWorkspaceSetStartup(&app.workspaceModel,"alpha",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK && settle(&app));
    xui_widget name=find(app.properties,xuiWidgetFindType(app.context,"input"),NULL),apply=find(app.properties,xuiWidgetFindType(app.context,"button"),"应用项目属性"),configure=find(app.properties,xuiWidgetFindType(app.context,"button"),"编辑构建、运行与测试配置…"); CHECK(name && apply && configure);
    CHECK(!xideActionQuery(&app,XIDE_ACTION_APPLY_PROPERTIES).enabled && !xuiWidgetGetEnabled(apply));
    version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xd->editor)); char original[100]; snprintf(original,sizeof(original),"%s",xuiEditGetText(name));
    CHECK(xuiSetFocusWidget(app.context,name)==XUI_OK && key(&app,'A',XUI_MOD_CTRL) && xuiInputText(app.context,'Q')==XUI_OK && settle(&app) && strcmp(xuiEditGetText(name),original));
    CHECK(key(&app,'Z',XUI_MOD_CTRL) && !strcmp(xuiEditGetText(name),original) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xd->editor))==version);
    CHECK(xuiEditSetText(name,"Beta 属性动作")==XUI_OK && settle(&app) && xideActionQuery(&app,XIDE_ACTION_APPLY_PROPERTIES).enabled && xuiWidgetGetEnabled(apply));
    CHECK(xuiScrollViewEnsureChildVisible(app.properties,apply)==XUI_OK && settle(&app) && pointer(&app,xuiWidgetGetWorldRect(apply)) && !strcmp(xideWorkspaceFindProject(&app.workspaceModel,"beta")->name,"Beta 属性动作") && !xideActionQuery(&app,XIDE_ACTION_APPLY_PROPERTIES).enabled && (xuiWidgetGetStateId(apply)&XUI_WIDGET_STATE_DISABLED) && !strcmp(app.workspaceModel.startupProject,"alpha"));
    CHECK(xideEditorShow(&app,xd)==XUI_OK && key(&app,XUI_KEY_F5,0)); xide_task* task=xideTaskViewSelected(&app);
    CHECK(task);
    CHECK(!strcmp(xideTaskProjectId(task),"alpha"));
    CHECK(!strcmp(app.selectedProject,"beta"));
    CHECK(waitPhase(&app,task,1));
    CHECK(xideActionQuery(&app,XIDE_ACTION_CLOSE_INPUT).enabled && allStates(&app));
    CHECK(xuiTerminalInputText(app.panels[XIDE_TERMINAL],"动作输入\n")==XUI_OK && menuAction(&app,app.fileMenu,XIDE_ACTION_CLOSE_INPUT) && !xideTaskInputOpen(task) && !xideActionQuery(&app,XIDE_ACTION_CLOSE_INPUT).enabled && !xideTaskWrite(task,"late",4) && waitPhase(&app,task,0));
    xide_task_info info={0}; xideTaskInfo(task,&info); CHECK(info.state==XIDE_TASK_SUCCEEDED && info.built && info.ran && !xideActionQuery(&app,XIDE_ACTION_STOP).enabled);
    CHECK(key(&app,XUI_KEY_F5,0)); task=xideTaskViewSelected(&app); CHECK(task && waitPhase(&app,task,1) && xideActionQuery(&app,XIDE_ACTION_STOP).enabled);
    CHECK(xuiScrollViewEnsureChildVisible(app.properties,configure)==XUI_OK && settle(&app) && pointer(&app,xuiWidgetGetWorldRect(configure)) && xideConfigurationViewIsOpen(&app) && !xideActionQuery(&app,XIDE_ACTION_BUILD).enabled && xideActionQuery(&app,XIDE_ACTION_STOP).enabled && allStates(&app));
    tasks=xideTasksCount(app.tasks); version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xd->editor)); CHECK(xideActionExecute(&app,XIDE_ACTION_NEW_FILE)==XIDE_ACTION_DISABLED && xideActionExecute(&app,XIDE_ACTION_THEME_CLASSIC)==XIDE_ACTION_DISABLED && key(&app,XUI_KEY_F7,0) && xideTasksCount(app.tasks)==tasks && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(xd->editor))==version);
    CHECK(key(&app,XUI_KEY_F5,XUI_MOD_SHIFT) && waitPhase(&app,task,0)); xideTaskInfo(task,&info); CHECK(info.state==XIDE_TASK_CANCELLED && xideConfigurationViewIsOpen(&app));
    xui_widget cancel=xideConfigurationViewControl(&app,XIDE_CONFIG_CANCEL); CHECK(xuiScrollViewEnsureChildVisible(xideDialogGetScrollViewWidget(xideConfigurationViewWindow(&app)),cancel)==XUI_OK && settle(&app) && pointer(&app,xuiWidgetGetWorldRect(cancel)) && !xideConfigurationViewIsOpen(&app) && allStates(&app));
    CHECK(!xideActionQuery(&app,XIDE_ACTION_REMOVE_FILE).enabled);
    xide_project* owner=xideWorkspaceFindProject(&app.workspaceModel,"beta"); CHECK(owner); size_t fileCount=owner->files.count; CHECK(xideProjectAddFile(owner,cPath,NULL,&error));
    CHECK(xideWorkspaceViewRefresh(&app)==XUI_OK && xideWorkspaceViewSelectFile(&app,"beta",owner->files.items[owner->files.count-1].id) && settle(&app) && xideActionQuery(&app,XIDE_ACTION_REMOVE_FILE).enabled);
    CHECK(xideActionExecute(&app,XIDE_ACTION_REMOVE_FILE)==XIDE_ACTION_DISPATCHED && settle(&app) && owner->files.count==fileCount && xrtFileExists(cPath) && cd->open);

    CHECK(editorCommands(&app));
    puts("actions_test passed: unique stable action IDs, native menu/context/toolbar state and actual row/save clicks, reordered binding, keyboard and targeted CodeEdit commands without reinjection, C/xlang checks and undo, read-only/unnamed/DLL guards, plain-field undo isolation, native-menu UIFont rebind/locked-save rollback, property/configuration buttons and reference-only removal, selected/startup separation, real child task stdin closure and modal Stop, 125% DPI and three native menu screenshots");
cleanup:
    if(began) xgeEnd();
    xideAppDestroy(&app); xideWorkspaceClear(&ws); free(root); free(entry); free(cPath); free(alpha); free(beta); free(workspace); free(settings); free(emptyImage); free(editImage); free(languageImage); xgeQuit(); return XGE_OK;
}
static DWORD WINAPI worker(void* data)
{ fixture* f=data; if(!SetThreadDesktop(f->desktop)) return 1; xge_desc_t d={0}; d.iWidth=1600; d.iHeight=1000; d.sTitle="xlang IDE action verification"; d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI; d.iRunMode=XGE_RUN_GAME_LOOP; int result=xgeInit(&d); if(result==XGE_OK) result=xgeRun(verify,f); xgeUnit(); return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{
    if(argc!=2 || !xrtDirCreateAll(argv[1])) return 2;
    char* directory=xidePathDirectory(argv[0]); char* compiler=directory ? xidePathResolve(directory,"task_test.exe") : NULL; free(directory); if(!compiler) return 1;
    fixture f={.directory=argv[1],.compiler=compiler}; wchar_t name[100]; swprintf(name,100,L"XlangActionVerification-%lu",GetCurrentProcessId()); f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL); if(!f.desktop) { free(compiler); return 1; }
    HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL); DWORD code=1; if(thread) { if(WaitForSingleObject(thread,90000)!=WAIT_OBJECT_0) { fputs("actions_test timed out\n",stderr); ExitProcess(1); } GetExitCodeThread(thread,&code); CloseHandle(thread); } CloseDesktop(f.desktop); free(compiler); return (int)code;
}
