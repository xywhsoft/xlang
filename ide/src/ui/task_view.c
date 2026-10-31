#include "ui/task_view.h"
#include "services/execution_target.h"
#include "ui/language_view.h"
#include "ui/completion_view.h"
#include "ui/c_completion_view.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "platform/path.h"
#include "platform/startup_alias.h"
#include "platform/settings.h"
#include "theme/theme.h"
#include "services/editor_font.h"
#include "lang/languages.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) return result_; } while(0)
#define VIEW_LOG_LIMIT (256u*1024u)
struct xide_task_view {
    xui_widget output,terminal,problems,list;
    xide_task *selected,*automatic;
    uint64_t logCursor,terminalCursor,listSignature;
    size_t diagnosticCount;
    char *compiler,*text,*display,**taskTitles,**problemTitles;
    uint64_t omitted;
    size_t textSize,taskTitleCount,problemTitleCount;
    unsigned char pending[4]; size_t pendingCount;
    int outputChanged,resetOutput,autoTerminal,autoInputProblems,settled,languageProblems,refreshingSelection;
    void* problemOwner; int (*problemJump)(xide_app*,size_t,void*);
    char status[1024];
};
static char* utf8(const wchar_t* text)
{
    int count=WideCharToMultiByte(CP_UTF8,0,text,-1,NULL,0,NULL,NULL); char* value=count ? malloc((size_t)count) : NULL;
    if(value) WideCharToMultiByte(CP_UTF8,0,text,-1,value,count,NULL,NULL);
    return value;
}
static void report(xide_app* app,const char* text)
{
    xideMessageShow(app,"xlang IDE",text,XUI_MSGBOX_ICON_ERROR);
}
static int pane(xide_app* app,int id)
{
    xui_dock_window_info_t info={0}; info.iSize=sizeof(info);
    return xuiDockPanelGetWindowInfo(app->dock,app->windows[id],&info)==XUI_OK && info.iState==XUI_DOCK_PANEL_WINDOW_DOCKED ? info.iPane : -1;
}
static int show(xide_app* app,int id)
{
    int current=pane(app,id);
    if(current<0) {
        for(int i=XIDE_HELP;i<=XIDE_USAGES && current<0;++i) current=pane(app,i);
        if(current>=0) TRY(xuiDockPanelDockWindowToPane(app->dock,app->windows[id],current));
        else { int editor=app->activeDocument ? app->activeDocument->window : app->windows[XIDE_XLANG_DEMO]; xui_dock_window_info_t info={0}; info.iSize=sizeof(info);
            TRY(xuiDockPanelGetWindowInfo(app->dock,editor,&info));
            TRY(xuiDockPanelDockWindowToPaneSide(app->dock,app->windows[id],info.iPane,XUI_DOCK_PANEL_SIDE_BOTTOM,0.28f,&current)); }
    }
    return xuiDockPanelSetPaneActiveWindow(app->dock,current,app->windows[id]);
}
static const char* stateText(enum xide_task_state state)
{
    const char* names[]={"排队","执行","取消中","成功","失败","已取消"}; return state>=XIDE_TASK_QUEUED && state<=XIDE_TASK_CANCELLED ? names[state] : "未知";
}
int xideTaskViewShow(xide_app* app) { return show(app,xideTaskViewSelected(app) ? XIDE_OUTPUT : XIDE_TASKS); }
static const char* phaseText(enum xide_task_phase phase)
{
    const char* names[]={"准备","工具链检查","构建","运行","结束","测试"}; return phase>=XIDE_TASK_PREPARE && phase<=XIDE_TASK_TEST ? names[phase] : "未知";
}
static const char* kindText(enum xide_task_kind kind)
{ return kind==XIDE_TASK_KIND_TEST ? "测试" : kind==XIDE_TASK_KIND_RUN ? "构建运行" : "构建"; }
static void clearTitles(char*** titles,size_t* count)
{
    for(size_t i=0;i<*count;++i) free((*titles)[i]);
    free(*titles); *titles=NULL; *count=0;
}
static void appendText(xide_task_view* view,const void* bytes,size_t size)
{
    if(size>VIEW_LOG_LIMIT) { bytes=(const unsigned char*)bytes+size-VIEW_LOG_LIMIT; size=VIEW_LOG_LIMIT; }
    if(view->textSize+size>VIEW_LOG_LIMIT) {
        size_t drop=view->textSize+size-VIEW_LOG_LIMIT;
        while(drop<view->textSize && ((unsigned char)view->text[drop]&0xc0)==0x80) ++drop;
        memmove(view->text,view->text+drop,view->textSize-drop); view->textSize-=drop; view->omitted+=drop;
    }
    memcpy(view->text+view->textSize,bytes,size); view->textSize+=size; view->text[view->textSize]=0; view->outputChanged=1;
}
static void displayBytes(xide_task_view* view,const unsigned char* bytes,size_t size,int final)
{
    /* Logs may be binary or split UTF-8 sequences. The TextEdit always receives
       valid UTF-8; raw files keep unmodified bytes for investigation. */
    unsigned char data[16388],normalized[49164]; size_t written=0; size_t count=view->pendingCount+size; memcpy(data,view->pending,view->pendingCount); memcpy(data+view->pendingCount,bytes,size); view->pendingCount=0;
    for(size_t i=0;i<count;) {
        unsigned ch=data[i]; size_t length=ch<0x80 ? 1 : ch>=0xc2 && ch<=0xdf ? 2 : ch>=0xe0 && ch<=0xef ? 3 : ch>=0xf0 && ch<=0xf4 ? 4 : 0;
        if(length && i+length>count && !final) { view->pendingCount=count-i; memcpy(view->pending,data+i,view->pendingCount); break; }
        int valid=length && i+length<=count;
        for(size_t j=1;valid && j<length;++j) if((data[i+j]&0xc0)!=0x80) valid=0;
        if(valid && length==3 && ((ch==0xe0 && data[i+1]<0xa0) || (ch==0xed && data[i+1]>=0xa0))) valid=0;
        if(valid && length==4 && ((ch==0xf0 && data[i+1]<0x90) || (ch==0xf4 && data[i+1]>=0x90))) valid=0;
        if(!valid) { memcpy(normalized+written,"\xef\xbf\xbd",3); written+=3; ++i; }
        else if(ch=='\r') { ++i; }
        else if(ch<32 && ch!='\n' && ch!='\t') { memcpy(normalized+written,"\xef\xbf\xbd",3); written+=3; i+=length; }
        else { memcpy(normalized+written,data+i,length); written+=length; i+=length; }
    }
    if(written) appendText(view,normalized,written);
}
xide_task* xideTaskViewSelected(xide_app* app) { return app->taskView ? app->taskView->selected : NULL; }
int xideTaskViewSelect(xide_app* app,xide_task* task)
{
    xide_task_view* view=app->taskView; if(!view || !xideTasksContains(app->tasks,task)) return XUI_ERROR_INVALID_ARGUMENT;
    view->languageProblems=0; view->problemOwner=NULL; view->problemJump=NULL; view->autoInputProblems=0; view->selected=task; view->settled=0; view->omitted=0; view->logCursor=view->terminalCursor=0; view->diagnosticCount=SIZE_MAX; view->textSize=view->pendingCount=0; view->text[0]=0; view->outputChanged=view->resetOutput=1;
    for(size_t i=0;i<xideTasksCount(app->tasks);++i) if(xideTasksAt(app->tasks,i)==task && i<(size_t)xuiTreeViewGetNodeCount(view->list)) {
        view->refreshingSelection=1; xuiTreeViewSetSelected(view->list,(int)i+1); view->refreshingSelection=0; break;
    }
    TRY(xuiTerminalFlush(view->terminal)); TRY(xuiTerminalClear(view->terminal)); TRY(xuiTerminalClearScrollback(view->terminal)); xgeRenderRequest(); return XUI_OK;
}
static void taskSelected(xui_widget widget,int id,void* user)
{
    (void)widget; xide_app* app=user; if(app->taskView && app->taskView->refreshingSelection) return;
    if(id>0) { xide_task* task=xideTasksAt(app->tasks,(size_t)id-1); if(task && task!=xideTaskViewSelected(app)) xideTaskViewSelect(app,task); }
}
int xideTaskViewJump(xide_app* app,size_t index)
{
    if(app->taskView && app->taskView->languageProblems) return app->taskView->problemJump ? app->taskView->problemJump(app,index,app->taskView->problemOwner) : xideLanguageViewJump(app,index);
    xide_task* task=xideTaskViewSelected(app); xide_task_info info={0}; xideTaskInfo(task,&info); if(!task) return 0;
    xide_diagnostic diagnostic={0}; xide_model_error error={0}; if(!xideTaskDiagnostic(task,index,&diagnostic)) return 0;
    if(info.invalidated && diagnostic.kind!=XIDE_DIAGNOSTIC_UNSAVED_INPUT) { xideDiagnosticClear(&diagnostic); return 0; }
    int okay=xideEditorOpen(app,diagnostic.path,&error);
    if(okay && diagnostic.kind==XIDE_DIAGNOSTIC_COMPILER) {
        xui_code_document document=xuiCodeEditGetDocument(app->activeDocument->editor); int start=0,end=0;
        int line=diagnostic.line-1,lines=xuiCodeDocumentGetLineCount(document); if(line>=lines) line=lines-1;
        if(xuiCodeDocumentGetLineRange(document,line,&start,&end)==XUI_OK) {
            const unsigned char* text=(const unsigned char*)xuiCodeEditGetText(app->activeDocument->editor);
            /* DEMO6 xlang columns count Unicode scalars (including tabs as one).
               C backend column conventions differ: use a reliable line start. */
            if(xidePathHasExtension(diagnostic.path,".xl")) for(int columns=1;start<end && columns<diagnostic.column;++columns) {
                ++start; while(start<end && (text[start]&0xc0)==0x80) ++start;
            }
            xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(app->activeDocument->editor),document,start,0); xuiCodeEditEnsureCaretVisible(app->activeDocument->editor);
        }
    }
    if(!okay) report(app,error.message);
    xideDiagnosticClear(&diagnostic); return okay;
}
static int problemEvent(xui_widget widget,const xui_event_t* event,void* user)
{
    if(event->iType==XUI_EVENT_POINTER_DOUBLE_CLICK || (event->iType==XUI_EVENT_KEY_DOWN && event->iKey==XUI_KEY_ENTER)) {
        int id=xuiTreeViewGetSelected(widget); if(id>0) { xide_app* app=user; int opened=xideTaskViewJump(app,(size_t)id-1); if(opened && app->taskView->languageProblems) return XUI_EVENT_DISPATCH_STOP; }
    }
    return XUI_OK;
}
static void terminalInput(xui_widget widget,const uint8_t* bytes,int size,void* user)
{
    xide_app* app=user; xide_task* task=xideTaskViewSelected(app);
    if(size==1 && bytes[0]==3) { xideTaskCancel(task); return; }
    if(size==1 && (bytes[0]==4 || bytes[0]==26)) { xideTaskCloseInput(task); return; }
    unsigned char* input=malloc((size_t)size); if(!input) return;
    for(int i=0;i<size;++i) input[i]=bytes[i]=='\r' ? '\n' : bytes[i];
    if(xideTaskWrite(task,input,(size_t)size)) xuiTerminalWrite(widget,bytes,size);
    else xuiTerminalWriteText(widget,"\r\n[当前没有可接收输入的运行会话，或输入队列已满]\r\n");
    free(input); xgeRenderRequest();
}
static char* discoverCompiler(const char* requested,const char* saved)
{
    if(requested && *requested) return xidePathResolve(NULL,requested);
    DWORD count=GetEnvironmentVariableW(L"XLANG_COMPILER",NULL,0);
    if(count) { wchar_t* value=malloc((size_t)count*sizeof(*value)); if(!value) return NULL; GetEnvironmentVariableW(L"XLANG_COMPILER",value,count); char* text=utf8(value); free(value); if(text) { char* path=xidePathResolve(NULL,text); free(text); return path; } }
    if(saved && *saved) return xidePathResolve(NULL,saved);
    wchar_t binary[32768]; if(!GetModuleFileNameW(NULL,binary,32768)) return NULL;
    char* text=utf8(xideStartupResourceModule(binary)); char* directory=text ? xidePathDirectory(text) : NULL; free(text);
    char* path=directory ? xidePathResolve(directory,"xl.exe") : NULL;
    if(path && !xidePathExists(path)) { free(path); path=directory ? xidePathResolve(directory,"../../release/xl.exe") : NULL; }
    free(directory);
    if(path && !xidePathExists(path)) { free(path); path=NULL; } return path;
}
int xideTaskViewCreate(xide_app* app)
{
    xide_task_view* view=calloc(1,sizeof(*view)); if(!view) return XUI_ERROR_OUT_OF_MEMORY; app->taskView=view;
    app->tasks=xideTasksCreate(); view->text=malloc(VIEW_LOG_LIMIT+1); view->display=malloc(VIEW_LOG_LIMIT+512); if(!app->tasks || !view->text || !view->display) return XUI_ERROR_OUT_OF_MEMORY; view->text[0]=0;
    xideAppLoadSettings(app);
    view->compiler=discoverCompiler(app->compilerPath,app->preferences.compiler);
    /* CodeEdit's wrapped viewport lays out visible rows. TextEdit re-shaped
       the whole retained log on each update, stalling the owner/UI thread. */
    xui_code_edit_desc_t output={0}; output.iSize=sizeof(output); output.pFont=app->codeFont; output.sText="构建输出和原始日志将在启动任务后显示。"; output.bReadonly=1; output.bWordWrap=1;
    TRY(xuiCodeEditCreate(app->context,&view->output,&output)); app->panels[XIDE_OUTPUT]=view->output;
    TRY(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(view->output))); TRY(xideConfigureEditorLanguage(view->output,"text"));
    xui_terminal_desc_t terminal={0}; terminal.iSize=sizeof(terminal); terminal.pFont=app->codeFont; terminal.iColumns=80; terminal.iRows=24; terminal.iScrollbackLimit=2000; terminal.iParseBudgetBytes=65536; xui_vec2_t cell={9,20}; if(app->codeFont && app->proxy.textMeasure) xideDpiTextMeasure(&app->proxy,app->codeFont," ",&cell); terminal.fCellWidth=cell.fX; terminal.fCellHeight=xideEditorLineHeight(app); terminal.fPadding=4;
    TRY(xuiTerminalCreate(app->context,&view->terminal,&terminal)); TRY(xuiTerminalSetInputCallback(view->terminal,terminalInput,app));
    TRY(xuiTerminalWriteText(view->terminal,"运行程序的管道会话：支持输入、输出与停止；Ctrl+D 关闭输入。\r\n")); app->panels[XIDE_TERMINAL]=view->terminal;
    xui_tree_view_desc_t tree={0}; tree.iSize=sizeof(tree); tree.pFont=app->uiFont; tree.fItemHeight=26; tree.fPadding=6; tree.fIndent=12;
    TRY(xuiTreeViewCreate(app->context,&view->list,&tree)); TRY(xuiTreeViewSetSelect(view->list,taskSelected,app)); app->panels[XIDE_TASKS]=view->list;
    TRY(xuiTreeViewCreate(app->context,&view->problems,&tree)); TRY(xuiWidgetSetEventHandler(view->problems,XUI_EVENT_POINTER_DOUBLE_CLICK,problemEvent,app)); TRY(xuiWidgetSetEventHandler(view->problems,XUI_EVENT_KEY_DOWN,problemEvent,app)); app->panels[XIDE_PROBLEMS]=view->problems;
    return XUI_OK;
}
void xideTaskViewDestroy(xide_app* app)
{
    xide_task_view* view=app->taskView; if(!view) return;
    clearTitles(&view->taskTitles,&view->taskTitleCount); clearTitles(&view->problemTitles,&view->problemTitleCount); free(view->compiler); free(view->text); free(view->display); free(view); app->taskView=NULL;
}
int xideTaskViewTheme(xide_app* app)
{
    xide_task_view* view=app->taskView; if(!view) return XUI_OK;
    xui_code_style_t style={sizeof(style),ideThemeColor(app->theme,IDE_TEXT),ideThemeColor(app->theme,IDE_FIELD),0};
    TRY(xuiCodeEditSetStyle(view->output,XUI_CODE_STYLE_DEFAULT,&style));
    return xuiTerminalSetColors(view->terminal,ideThemeColor(app->theme,IDE_FIELD),ideThemeColor(app->theme,IDE_TEXT),ideThemeColor(app->theme,IDE_TEXT),ideThemeColor(app->theme,IDE_SELECTION),ideThemeColor(app->theme,IDE_TEXT),ideThemeColor(app->theme,IDE_ACCENT),ideThemeColor(app->theme,IDE_ACCENT),ideThemeColor(app->theme,IDE_ACCENT));
}
static void refreshTrees(xide_app* app)
{
    xide_task_view* view=app->taskView; size_t count=xideTasksCount(app->tasks); uint64_t signature=count;
    for(size_t i=0;i<count;++i) { xide_task_info info={0}; xideTaskInfo(xideTasksAt(app->tasks,i),&info); signature=signature*131+info.id*17+(unsigned)info.state*7+(unsigned)info.phase; }
    if(signature!=view->listSignature && count<=INT_MAX) {
        char** titles=calloc(count+1,sizeof(*titles)); xui_tree_view_node_t* nodes=calloc(count+1,sizeof(*nodes)); int okay=titles && nodes;
        for(size_t i=0;okay && i<count;++i) { xide_task* task=xideTasksAt(app->tasks,i); xide_task_info info={0}; xideTaskInfo(task,&info); titles[i]=malloc(strlen(xideTaskProjectName(task))+strlen(xideTaskConfigurationId(task))+192); if(!titles[i]) { okay=0; break; }
            sprintf(titles[i],"#%llu · %s · %s · %s · %s/%s · %llu ms",(unsigned long long)info.id,kindText(info.kind),xideTaskProjectName(task),xideTaskConfigurationId(task),stateText(info.state),phaseText(info.phase),(unsigned long long)info.durationMs); nodes[i].bEnabled=1; nodes[i].iId=(int)i+1; nodes[i].iParent=-1; nodes[i].sText=titles[i]; }
        view->refreshingSelection=1;
        if(okay && xuiTreeViewSetNodes(view->list,nodes,(int)count)==XUI_OK) {
            clearTitles(&view->taskTitles,&view->taskTitleCount); view->taskTitles=titles; view->taskTitleCount=count; view->listSignature=signature;
            int selected=0; for(size_t i=0;i<count;++i) if(xideTasksAt(app->tasks,i)==view->selected) { selected=(int)i+1; break; }
            xuiTreeViewSetSelected(view->list,selected);
        }
        else { if(titles) { for(size_t i=0;i<count;++i) free(titles[i]); free(titles); } } free(nodes);
        view->refreshingSelection=0;
    }
    if(view->languageProblems) return;
    count=xideTaskDiagnosticCount(view->selected);
    if(count!=view->diagnosticCount && count<=INT_MAX) {
        char** titles=calloc(count+1,sizeof(*titles)); xui_tree_view_node_t* nodes=calloc(count+1,sizeof(*nodes)); int okay=titles && nodes;
        for(size_t i=0;okay && i<count;++i) { xide_diagnostic diagnostic={0}; if(!xideTaskDiagnostic(view->selected,i,&diagnostic)) { okay=0; break; }
            titles[i]=malloc(2*strlen(diagnostic.path)+strlen(diagnostic.message)+128); if(titles[i]) {
                if(diagnostic.kind==XIDE_DIAGNOSTIC_UNSAVED_INPUT) {
                    const char* name=strrchr(diagnostic.path,'/'); name=name ? name+1 : diagnostic.path;
                    sprintf(titles[i],"提交时未保存 · %s · %s · %s",name,diagnostic.message,diagnostic.path);
                } else sprintf(titles[i],"%s:%d:%d · %s",diagnostic.path,diagnostic.line,diagnostic.column,diagnostic.message);
                nodes[i].bEnabled=1; nodes[i].iId=(int)i+1; nodes[i].iParent=-1; nodes[i].sText=titles[i];
            } else okay=0; xideDiagnosticClear(&diagnostic); }
        if(okay && xuiTreeViewSetNodes(view->problems,nodes,(int)count)==XUI_OK) { clearTitles(&view->problemTitles,&view->problemTitleCount); view->problemTitles=titles; view->problemTitleCount=count; view->diagnosticCount=count; }
        else { if(titles) { for(size_t i=0;i<count;++i) free(titles[i]); free(titles); } } free(nodes);
    }
}
void xideTaskViewPoll(xide_app* app)
{
    xide_task_view* view=app->taskView; if(!view) return;
    for(size_t i=0;i<app->workspaceModel.projectCount;++i) if(app->workspaceModel.projects[i].dirty) xideTasksInvalidateInput(app->tasks,app->workspaceModel.projects[i].manifest);
    xideTasksPoll(app->tasks);
    const xide_task* protectedTasks[]={view->selected,view->automatic};
    xideTasksTrimHistory(app->tasks,XIDE_TASK_HISTORY_LIMIT,protectedTasks,2); refreshTrees(app);
    int busy=xideTasksBusy(app->tasks); xideActionsRefresh(app);
    if(view->selected) {
        xide_task_info info={0}; xideTaskInfo(view->selected,&info); unsigned char bytes[16384]; uint64_t dropped=0; size_t size;
        int drained=0;
        for(int i=0;i<4;++i) {
            size=xideTaskReadLog(view->selected,&view->logCursor,bytes,sizeof(bytes),&dropped);
            if(!size) { drained=1; break; }
            if(dropped) { view->pendingCount=0; appendText(view,"\n[部分显示日志已省略；完整输出保存在任务原始日志]\n",strlen("\n[部分显示日志已省略；完整输出保存在任务原始日志]\n")); }
            displayBytes(view,bytes,size,0); xgeRenderRequest();
        }
        if(drained && info.phase==XIDE_TASK_DONE && view->pendingCount) displayBytes(view,bytes,0,1);
        int terminalDrained=0;
        for(int i=0;i<4;++i) {
            size=xideTaskReadTerminal(view->selected,&view->terminalCursor,bytes,sizeof(bytes),&dropped);
            if(!size) { terminalDrained=1; break; }
            if(dropped) xuiTerminalWriteText(view->terminal,"\r\n[部分终端回滚已省略；参见运行原始日志]\r\n");
            xuiTerminalWrite(view->terminal,bytes,(int)size); xgeRenderRequest();
        }
        view->settled=info.phase==XIDE_TASK_DONE && drained && terminalDrained && !view->pendingCount;
        if(!view->settled) xgeRenderRequest();
        if(view->automatic==view->selected && (info.phase==XIDE_TASK_RUN || info.ran || info.phase==XIDE_TASK_TEST || info.tested) && !view->autoTerminal) { show(app,XIDE_TERMINAL); xuiLayout(app->context); xuiSetFocusWidget(app->context,view->terminal); view->autoTerminal=1; }
        if(view->automatic==view->selected && info.phase==XIDE_TASK_DONE && !view->autoInputProblems) {
            view->autoInputProblems=1;
            if(info.invalidated) for(size_t i=0;i<xideTaskDiagnosticCount(view->selected);++i) {
                xide_diagnostic diagnostic={0}; int copied=xideTaskDiagnostic(view->selected,i,&diagnostic);
                int pending=copied && diagnostic.kind==XIDE_DIAGNOSTIC_UNSAVED_INPUT; xideDiagnosticClear(&diagnostic);
                if(pending) {
                    view->autoInputProblems=2; view->languageProblems=0; view->problemOwner=NULL; view->problemJump=NULL; view->diagnosticCount=SIZE_MAX;
                    refreshTrees(app); show(app,XIDE_PROBLEMS); xuiLayout(app->context); break;
                }
            }
        }
        const char* coverage=info.kind==XIDE_TASK_KIND_TEST ? "" : info.inputReport ? (info.inputsVerified ? " · 实际依赖已核验" : " · 实际依赖待核验") : info.built ? " · 直接文件与资源守护" : "";
        snprintf(view->status,sizeof(view->status),"%s任务 #%llu · %s%s / %s · %llu ms%s%s",kindText(info.kind),(unsigned long long)info.id,stateText(info.state),coverage,phaseText(info.phase),(unsigned long long)info.durationMs,*info.error ? " · " : "",info.error);
        xuiStatusBarSetItemText(app->statusBar,0,view->status);
    }
    int selectionStart=0,selectionEnd=0;
    int selecting=xuiGetFocusWidget(app->context)==view->output &&
        xuiCodeSelectionGetRange(xuiCodeEditGetSelection(view->output),&selectionStart,&selectionEnd)==XUI_OK && selectionStart!=selectionEnd;
    /* Keep the visible snapshot stable while the user selects/copies it. Raw
       capture and the bounded tail continue; clearing selection resumes it. */
    if(view->outputChanged && (!selecting || view->resetOutput)) {
        size_t prefix=view->omitted ? (size_t)snprintf(view->display,512,"[显示已省略 %llu 字节；完整输出保存在任务原始日志]\n",(unsigned long long)view->omitted) : 0;
        memcpy(view->display+prefix,view->text,view->textSize+1);
        if(xuiCodeEditSetText(view->output,view->textSize ? view->display : "等待任务输出…")==XUI_OK &&
           xideRefreshEditorSyntax(view->output)==XUI_OK) view->outputChanged=view->resetOutput=0;
    }
    if(busy) xgeRenderRequest();
}
int xideTaskViewSettled(xide_app* app)
{
    return !xideTasksBusy(app->tasks) && (!app->taskView || !app->taskView->selected || app->taskView->settled);
}
int xideTaskViewCanBuildFile(xide_app* app)
{
    xide_document* d=app ? app->activeDocument : NULL;
    return d && d->open && (d->path ? xidePathHasExtension(d->path,".c") || xidePathHasExtension(d->path,".xl") :
        !xuiCodeEditIsReadonly(d->editor) && (!strcmp(d->language,"c") || !strcmp(d->language,"xlang")));
}
int xideTaskViewAction(xide_app* app,int action)
{
    if((action<XIDE_ACTION_BUILD || action>XIDE_ACTION_CLOSE_INPUT) && action!=XIDE_ACTION_TEST && action!=XIDE_ACTION_BUILD_FILE && action!=XIDE_ACTION_RUN_FILE && action!=XIDE_ACTION_STOP_ALL) return 0;
    xide_task_view* view=app->taskView; if(!view) return 1;
    if(action==XIDE_ACTION_STOP) { xideTaskCancel(view->selected); return 1; }
    if(action==XIDE_ACTION_STOP_ALL) { for(size_t i=0;i<xideTasksCount(app->tasks);++i) xideTaskCancel(xideTasksAt(app->tasks,i)); return 1; }
    if(action==XIDE_ACTION_CLOSE_INPUT) { xideTaskCloseInput(view->selected); return 1; }
    if(action==XIDE_ACTION_COMPILER) {
        char* compiler=xideMessageChoosePath(app,XUI_FILE_DIALOG_MODE_OPEN_FILE,"选择 xlang 编译器","可执行程序|*.exe;*|所有文件|*.*",view->compiler,NULL,UINT64_C(720),0);
        if(compiler) { xide_model_error error={0}; if(!xideSettingsSetCompiler(&app->preferences,compiler,&error)) { report(app,error.message);free(compiler); } else { free(view->compiler); view->compiler=compiler; } } return 1;
    }
    int run=action==XIDE_ACTION_RUN || action==XIDE_ACTION_RUN_FILE,test=action==XIDE_ACTION_TEST;
    xide_execution_context context;xideExecutionResolve(app,action==XIDE_ACTION_BUILD_FILE || action==XIDE_ACTION_RUN_FILE,&context);
    xide_action_state allowed=xideExecutionQuery(app,action);
    if(!allowed.enabled) { report(app,allowed.reason);app->taskExit=1;return 1; }
    int file=context.file;
    xide_project* project=context.project;xide_model_error error={0};
    if(file) {
        if(!view->compiler || !*view->compiler || !xideTaskViewCanBuildFile(app)) { report(app,"请打开 C/xlang 源文件并选择编译器。"); app->taskExit=1; return 1; }
        xide_document* document=app->activeDocument;
        if(!xideEditorSaveForTask(app,document)) { if(!xideMessagePending(app)) app->taskExit=1; return 1; }
        if(!xideTaskViewCanBuildFile(app) || !document->path) { report(app,"单文件构建需要保存为 .c 或 .xl 文件。"); app->taskExit=1; return 1; }
        xide_strings pending={0}; pending.items=calloc(app->documents.count+1,sizeof(*pending.items));
        if(!pending.items) { report(app,"无法冻结单文件构建的输入保护。"); app->taskExit=1; return 1; }
        for(size_t i=0;i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(d->open && d->dirty && d->path) pending.items[pending.count++]=d->path; }
        xide_task* task=xideTasksBuildFileGuarded(app->tasks,document->path,view->compiler,run,&pending,&error); free(pending.items);
        if(!task) { report(app,error.message); app->taskExit=1; return 1; }
        xideTaskViewSelect(app,task); view->automatic=task; view->autoTerminal=0; view->autoInputProblems=0; app->taskExit=0; show(app,XIDE_OUTPUT); xgeRenderRequest(); return 1;
    }
    if(!project || !project->available || (!test && !view->compiler)) { report(app,test ? "请选择可用项目并配置测试程序。" : "请选择可用项目和 xlang 编译器；运行还需要设置启动项目。"); app->taskExit=1; return 1; }
    char* selected=malloc(strlen(project->id)+1);
    char* configuration=malloc(strlen(context.configurationId)+1);
    if(!selected || !configuration) { free(selected);free(configuration);app->taskExit=1;return 1; }
    strcpy(selected,project->id);strcpy(configuration,context.configurationId);
    if(!xideEditorSaveForProject(app,project)) { free(selected);free(configuration); if(!xideMessagePending(app)) app->taskExit=1; return 1; }
    project=xideWorkspaceFindProject(&app->workspaceModel,selected); free(selected);
    if(!project || !project->available || !xideProjectFindConfiguration(project,configuration)) { free(configuration);report(app,"保存清单后，目标项目或配置已失效。"); app->taskExit=1; return 1; }
    if(project->dirty && !xideProjectSave(project,NULL,&error)) { free(configuration);report(app,error.message); app->taskExit=1; return 1; }
    xideWorkspaceViewRefresh(app);
    xide_strings pending={0};
    if(!test) {
        pending.items=calloc(app->documents.count+1,sizeof(*pending.items));
        if(!pending.items) { free(configuration);report(app,"无法冻结未保存文档的输入保护；请稍后重试。"); app->taskExit=1; return 1; }
        for(size_t i=0;i<app->documents.count;++i) {
            xide_document* d=app->documents.items[i];
            if(d->open && d->dirty && d->path) pending.items[pending.count++]=d->path;
        }
    }
    xide_task* task=test ? xideTasksTest(app->tasks,project,configuration,&error) : xideTasksBuildGuarded(app->tasks,project,configuration,view->compiler,run,&pending,&error);
    free(pending.items);free(configuration);
    if(!task) { report(app,error.message); app->taskExit=1; return 1; }
    xideTaskViewSelect(app,task); view->automatic=task; view->autoTerminal=0; view->autoInputProblems=0; app->taskExit=0; show(app,XIDE_OUTPUT); xgeRenderRequest(); return 1;
}

const char* xideTaskViewCompiler(xide_app* app) { return app->taskView ? app->taskView->compiler : NULL; }

int xideTaskViewShowLanguageProblemsRouted(xide_app* app,const char* scope,const char* const* titles,size_t count,int activate,void* owner,int (*jump)(xide_app*,size_t,void*))
{
 xide_task_view* view=app->taskView; if(!view || !scope || count>4096 || (count && !titles)) return XUI_ERROR_INVALID_ARGUMENT;
 /* Keep a newly blocked build actionable while passive analysis updates arrive.
    An explicit check or task selection still chooses the displayed owner. */
 if(!activate && view->automatic==view->selected && view->autoInputProblems==2) return XUI_OK;
 if(activate) view->autoInputProblems=1;
 char** owned=calloc(count+1,sizeof(*owned)); xui_tree_view_node_t* nodes=calloc(count+1,sizeof(*nodes)); int okay=owned && nodes;
 if(okay) { owned[0]=malloc(strlen(scope)+1); if(owned[0]) strcpy(owned[0],scope); else okay=0; nodes[0]=(xui_tree_view_node_t){.iId=0,.iParent=-1,.sText=owned[0],.bEnabled=1}; }
 for(size_t i=0;okay && i<count;++i) { owned[i+1]=malloc(strlen(titles[i])+1); if(owned[i+1]) strcpy(owned[i+1],titles[i]); else okay=0; nodes[i+1]=(xui_tree_view_node_t){.iId=(int)i+1,.iParent=0,.sText=owned[i+1],.bEnabled=1}; }
 int result=okay ? xuiTreeViewSetNodes(view->problems,nodes,(int)count+1) : XUI_ERROR_OUT_OF_MEMORY;
 if(result==XUI_OK) { clearTitles(&view->problemTitles,&view->problemTitleCount); view->problemTitles=owned; view->problemTitleCount=count+1; owned=NULL; view->languageProblems=1; view->problemOwner=owner; view->problemJump=jump; xuiTreeViewSetNodeExpanded(view->problems,0,1); if(activate && !xideCompletionViewActive(app) && !xideCCompletionViewActive(app)) result=show(app,XIDE_PROBLEMS); }
 if(owned) { for(size_t i=0;i<=count;++i) free(owned[i]); free(owned); } free(nodes); xgeRenderRequest(); return result;
}
int xideTaskViewShowLanguageProblemsScoped(xide_app* app,const char* scope,const char* const* titles,size_t count,int activate)
{ return xideTaskViewShowLanguageProblemsRouted(app,scope,titles,count,activate,app->languageView,NULL); }
void* xideTaskViewLanguageProblemsOwner(xide_app* app)
{ return app && app->taskView && app->taskView->languageProblems ? app->taskView->problemOwner : NULL; }
void xideTaskViewClearLanguageProblemsOwned(xide_app* app,void* owner)
{ xide_task_view* view=app ? app->taskView : NULL; if(view && view->languageProblems && view->problemOwner==owner) { xuiTreeViewSetNodes(view->problems,NULL,0); clearTitles(&view->problemTitles,&view->problemTitleCount); view->languageProblems=0; view->problemOwner=NULL; view->problemJump=NULL; view->diagnosticCount=SIZE_MAX; } }
void xideTaskViewClearLanguageProblems(xide_app* app)
{ if(app) xideTaskViewClearLanguageProblemsOwned(app,app->languageView); }

int xideTaskViewShowLanguageProblems(xide_app* app,const char* const* titles,size_t count,int activate)
{ return xideTaskViewShowLanguageProblemsScoped(app,"保存后的 xlang 单入口检查（未保存依赖和 C 语义尚未接入）",titles,count,activate); }
