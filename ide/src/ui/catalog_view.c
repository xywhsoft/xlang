#include "ui/catalog_view.h"
#include "platform/dpi_proxy.h"
#include "ui/completion_view.h"
#if XIDE_ENABLE_C_LSP
#include "ui/c_completion_view.h"
#endif
#include "ui/references_view.h"
#include "ui/reading_state.h"
#include "services/ui_font.h"
#include "ui/editor_view.h"
#include "ui/help_document.h"
#include "lang/languages.h"
#include "theme/theme.h"
#include "platform/path.h"
#include "platform/startup_alias.h"
#include "services/editor_font.h"
#include "services/catalog_sources.h"
#include "services/catalog_lookup.h"
#include "services/catalog_job.h"
#include "ui/workbench_windows.h"
#include <math.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) return result_; } while(0)
typedef struct follow_binding follow_binding;
struct xide_catalog_view {
    xide_catalog_job* load; int settled,publishing; char* deferredSelection; double loadMs,publishMs;
    xui_widget helpPanel,panel,search,count,metadata,parameters,exampleHeading,insert,status,reload,returnReferences;
    xide_help_view* helpDocument;
    char* catalogFile; size_t detailCount; char loadError[600];
    const xide_command* selected;
    char **details,*query;
    size_t visible;
    int tableWidth,tableMode;
    xui_widget pendingEditor; uint64_t pendingDocument; uint32_t pendingVersion; int pendingOffset; char pendingLanguage[16];
    xui_widget sourceButtons[2],sourceWindow,sourceContent,sourceTitle,sourceStatus,sourceEditor,sourceClose;
    char sourceMessage[600];
    xide_catalog_lookup* lookup;
    xui_widget followBar,followNotice,followPin,followChoices,composingEditor;
    follow_binding* followBindings;
    xui_widget observedEditor; xui_code_document observedCore;
    uint32_t observedVersion;int observedStart,observedEnd,pinned,followPending,followUpdating,helpRetry;
    uint64_t followAt,followRevision,observedDocument;char observedLanguage[16];uint32_t choices[64];size_t choiceCount,choiceTotal;
};
static int selectCommand(xide_app*,size_t,int);
#include "ui/catalog_follow.inc"
static int label(xide_app* app,xui_widget* widget,const char* text,int muted);
static void sourceLayout(xide_app* app)
{
    xide_catalog_view* view=app->catalogView; if(!view || !view->sourceWindow) return;
    xideDialogSetMaxSize(view->sourceWindow,app->viewportWidth-32,app->viewportHeight-32);
}
static int sourceTheme(xide_app* app)
{
    if(!app->catalogView || !app->catalogView->sourceEditor) return XUI_OK;
    const int ids[]={XUI_CODE_STYLE_DEFAULT,XUI_CODE_STYLE_KEYWORD,XUI_CODE_STYLE_TYPE,XUI_CODE_STYLE_NUMBER,XUI_CODE_STYLE_STRING,XUI_CODE_STYLE_CHAR,XUI_CODE_STYLE_COMMENT};
    const ide_color_role roles[]={IDE_TEXT,IDE_KEYWORD,IDE_TYPE,IDE_NUMBER,IDE_STRING,IDE_STRING,IDE_COMMENT};
    for(size_t i=0;i<sizeof(ids)/sizeof(*ids);++i) { xui_code_style_t style={sizeof(style),ideThemeColor(app->theme,roles[i]),ideThemeColor(app->theme,IDE_FIELD),0}; TRY(xuiCodeEditSetStyle(app->catalogView->sourceEditor,ids[i],&style)); }
    return XUI_OK;
}
void xideCatalogSourceClose(xide_app* app)
{ if(app && app->catalogView && app->catalogView->sourceWindow) { xideDialogSetOpen(app->catalogView->sourceWindow,0); xgeRenderRequest(); } }
static void sourceClosed(xui_widget widget,void* user) { (void)widget; xideCatalogSourceClose(user); }
static void sourceChanged(xui_widget widget,int open,void* user) { (void)widget; (void)open; xideActionsRefresh(user); xgeRenderRequest(); }
static int sourceCreate(xide_app* app)
{
    xide_catalog_view* view=app->catalogView;
    if(view->sourceEditor) return XUI_OK;
    /* Lazy allocation keeps unopened source viewers out of startup and sessions. */
    if(view->sourceWindow) { xideDialogDestroy(view->sourceWindow); view->sourceWindow=NULL; }
    TRY(xideDialogCreate(app,&view->sourceWindow,"来源文件（只读）",920,680,14)); view->sourceContent=xideDialogGetContentWidget(view->sourceWindow);
    TRY(xuiWidgetAddChild(app->root,view->sourceWindow)); TRY(xuiWidgetSetLayoutType(view->sourceContent,XUI_LAYOUT_COLUMN)); TRY(xuiWidgetSetGap(view->sourceContent,8));
    TRY(label(app,&view->sourceTitle,"来源文件（只读）",0)); TRY(label(app,&view->sourceStatus,"",1));
    TRY(xuiWidgetAddChild(view->sourceContent,view->sourceTitle)); TRY(xuiWidgetAddChild(view->sourceContent,view->sourceStatus));
    xui_code_edit_desc_t code={0}; code.iSize=sizeof(code); code.pFont=app->codeFont; code.sText=""; code.bReadonly=1; code.bShowLineNumbers=1; code.iTabColumns=4; code.iIndentColumns=4;
    xui_widget editor=NULL; TRY(xideDpiCodeEditCreate(app->context,&editor,&code));
    TRY(xuiWidgetAddChild(view->sourceContent,editor)); TRY(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(editor))); TRY(xideConfigureEditorLanguage(editor,"text"));
    xui_button_desc_t button={0}; button.iSize=sizeof(button); button.pFont=app->uiFont; button.sText="关闭来源查看";
    TRY(xuiButtonCreate(app->context,&view->sourceClose,&button)); TRY(xuiButtonSetClick(view->sourceClose,sourceClosed,app));
    xui_widget children[]={view->sourceTitle,view->sourceStatus,editor,view->sourceClose};
    for(int i=0;i<4;++i) TRY(xuiWidgetSetSizeMode(children[i],XUI_SIZE_FILL,i==2 ? XUI_SIZE_FILL : XUI_SIZE_FIXED));
    TRY(xuiWidgetAddChild(view->sourceContent,view->sourceClose));
    TRY(xideDialogSetCancel(view->sourceWindow,sourceClosed,app)); TRY(xuiWidgetSetCancelAction(editor,sourceClosed,app));
    TRY(xideDialogSetChange(view->sourceWindow,sourceChanged,app)); TRY(xideUiFontBindWidget(app,view->sourceWindow));
    view->sourceEditor=editor;TRY(xideDialogFormBegin(view->sourceWindow,560,380));
    TRY(xideDialogFormBlock(view->sourceWindow,view->sourceTitle,0,0));TRY(xideDialogFormBlock(view->sourceWindow,view->sourceStatus,0,0));
    TRY(xideDialogFormBlock(view->sourceWindow,editor,280,1));
    TRY(xideDialogFormActions(view->sourceWindow,&view->sourceClose,1));TRY(xideDialogFormEnd(view->sourceWindow));
    TRY(sourceTheme(app)); return XUI_OK;
}
static const char* selectedSource(xide_app* app,int document,int* line)
{
    if(!app || !app->catalogView || !app->catalogView->selected) return NULL;
    const xide_command* command=app->catalogView->selected;
    if(document==XIDE_CATALOG_SOURCE_DECLARATION) { if(line) *line=command->sourceLine; return command->source; }
    if(document!=XIDE_CATALOG_SOURCE_DOCUMENT) return NULL;
    const xide_help_document* help=xideHelpSourceDocument(app->catalogView->helpDocument);
    if(!help) return NULL;
    if(line) *line=help->sourceLine;
    return help->source;
}
int xideCatalogSourceAvailable(xide_app* app,int document) { return selectedSource(app,document,NULL)!=NULL; }
int xideCatalogSourceIsOpen(xide_app* app) { return app && app->catalogView && app->catalogView->sourceWindow && xideDialogIsOpen(app->catalogView->sourceWindow); }
int xideCatalogSourceOpen(xide_app* app,int document)
{
    int line=1; const char* source=selectedSource(app,document,&line);
    if(!source || xideActionsHasModal(app) || sourceCreate(app)!=XUI_OK) return 0;
    xide_catalog_view* view=app->catalogView; xide_model_error error={0}; xide_text_file file={0};
    int valid=xideCatalogSourceRead(&app->catalog,source,&file,&error); size_t size=strlen(source)+80; char* title=malloc(size); if(!title) { xideTextFileClear(&file); return 0; }
    snprintf(title,size,"来源文件（只读）：%s:%d",source,line); xuiLabelSetText(view->sourceTitle,title); free(title);
    const char* language=xidePathHasExtension(source,".xl") ? "xlang" : xidePathHasExtension(source,".c") || xidePathHasExtension(source,".h") || xidePathHasExtension(source,".inc") ? "c" : "text";
    int result=xuiCodeEditSetText(view->sourceEditor,valid ? file.text : ""); xideTextFileClear(&file);
    if(result!=XUI_OK || xideConfigureEditorLanguage(view->sourceEditor,language)!=XUI_OK) return 0;
    xui_code_document core=xuiCodeEditGetDocument(view->sourceEditor); int lines=xuiCodeDocumentGetLineCount(core),offset=0;
    if(valid && (line<1 || line>lines)) { valid=0; snprintf(error.message,sizeof(error.message),"来源文件已校验，但目录行号超出文件范围；请检查命令目录。"); }
    if(valid && xuiCodeDocumentLineColumnToOffset(core,line-1,0,&offset)!=XUI_OK) return 0;
    if(xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(view->sourceEditor),core,offset,0)!=XUI_OK) return 0;
    snprintf(view->sourceMessage,sizeof(view->sourceMessage),"%s",valid ? "已校验的目录来源快照 · 只读；关闭后返回原文档，编辑草稿保持不变。" : error.message);
    xuiLabelSetText(view->sourceStatus,view->sourceMessage); ideThemeSetTextRole(view->sourceStatus,valid ? IDE_MUTED : IDE_WARNING);
    sourceLayout(app); xideDialogSetScroll(view->sourceWindow,0,0); xideDialogSetFocusRestore(view->sourceWindow,xuiGetFocusWidget(app->context));
    xideDialogSetOpen(view->sourceWindow,1); xuiSetFocusWidget(app->context,view->sourceEditor); xuiLayout(app->context); xuiCodeEditSetScroll(view->sourceEditor,0,0); xuiCodeEditEnsureCaretVisible(view->sourceEditor); xgeRenderRequest(); return 1;
}
xui_widget xideCatalogSourceButton(xide_app* app,int document) { return app && app->catalogView && document>=0 && document<2 ? app->catalogView->sourceButtons[document] : NULL; }
xui_widget xideCatalogSourceWindow(xide_app* app) { return app && app->catalogView ? app->catalogView->sourceWindow : NULL; }
xui_widget xideCatalogSourceEditor(xide_app* app) { return app && app->catalogView ? app->catalogView->sourceEditor : NULL; }
xui_widget xideCatalogSourceCloseWidget(xide_app* app) { return app && app->catalogView ? app->catalogView->sourceClose : NULL; }
const char* xideCatalogSourceMessage(xide_app* app) { return app && app->catalogView ? app->catalogView->sourceMessage : ""; }
static int label(xide_app* app,xui_widget* widget,const char* text,int muted)
{
    xui_label_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; desc.sText=text;
    desc.iWrapMode=XUI_TEXT_WRAP_WORD; desc.iTextFlags=XUI_TEXT_ALIGN_LEFT|XUI_TEXT_ALIGN_TOP|XUI_TEXT_CLIP; desc.fLineGap=3;
    TRY(xuiLabelCreate(app->context,widget,&desc)); return ideThemeSetTextRole(*widget,muted ? IDE_MUTED : IDE_TEXT);
}
static const char* requirement(const xide_command_parameter* parameter)
{
    if(!strcmp(parameter->type,"...") || !strcmp(parameter->type,"c_vararg")) return "变参";
    return parameter->optional ? "可选" : "必需";
}
static int parameterCount(xui_widget widget,void* user)
{
    (void)widget; xide_catalog_view* view=user; return view->selected ? (int)view->selected->parameterCount : 0;
}
static int parameterCell(xui_widget widget,int row,int column,xui_table_view_cell_t* cell,void* user)
{
    (void)widget; xide_catalog_view* view=user;
    if(!view->selected || row<0 || (size_t)row>=view->selected->parameterCount || !cell) return XUI_ERROR_INVALID_ARGUMENT;
    const xide_command_parameter* p=&view->selected->parameters[row];
    const char* values[]={p->name,p->type,requirement(p),p->defaultValue,p->description};
    if(column<0 || column>=5) return XUI_ERROR_INVALID_ARGUMENT;
    cell->sText=*values[column] ? values[column] : "—"; cell->sTooltip=cell->sText; return XUI_OK;
}
static int validTarget(xide_app* app)
{
    const xide_command* selected=app->catalogView ? app->catalogView->selected : NULL;
    return selected && *selected->example && app->activeDocument && app->activeDocument->open &&
        !xuiCodeEditIsReadonly(app->activeDocument->editor) && !strcmp(selected->language,xuiCodeEditGetLanguage(app->activeDocument->editor));
}
int xideCatalogViewInsertExample(xide_app* app)
{
    if(!app || !validTarget(app)) return 0;
    xui_widget editor=app->activeDocument->editor;
    xuiCodeEditCancelCompletion(editor);
    if(xuiCodeEditingInsertText(xuiCodeEditGetDocument(editor),xuiCodeEditGetSelection(editor),app->catalogView->selected->example,0)!=XUI_OK) return 0;
    xuiCodeEditEnsureCaretVisible(editor); xuiSetFocusWidget(app->context,editor); xgeRenderRequest(); return 1;
}
static void returnReferencesClicked(xui_widget widget,void* user) { (void)widget; xideReferencesViewOpen(user); }
static void insertClicked(xui_widget widget,void* user) { (void)widget; xideCatalogViewInsertExample(user); }
static char* parametersText(const xide_command* command)
{
    size_t size=32;
    for(size_t i=0;i<command->parameterCount;++i) {
        const xide_command_parameter* p=&command->parameters[i]; size+=strlen(p->name)+strlen(p->type)+strlen(p->defaultValue)+strlen(p->description)+96;
    }
    char* result=malloc(size); if(!result) return NULL; strcpy(result,"参数\n");
    for(size_t i=0;i<command->parameterCount;++i) {
        const xide_command_parameter* p=&command->parameters[i]; size_t at=strlen(result);
        snprintf(result+at,size-at,"%s  %s · %s%s%s%s%s\n",p->name,p->type,requirement(p),*p->defaultValue ? " · 默认 " : "",p->defaultValue,*p->description ? "\n" : "",p->description);
    }
    return result;
}
static int selectCommand(xide_app* app,size_t index,int automatic)
{
    if(!app || !app->catalogView || index>=app->catalog.count) return 0;
    xide_catalog_view* view=app->catalogView; const xide_command* command=&app->catalog.entries[index]; int changed=view->selected!=command;
    if(!changed)return 1;
    view->selected=command; view->tableMode=-1; app->selectedCommand=(int)index;
    if(!automatic && xuiTreeViewGetSelected(app->commands)!=100+(int)index && xideCatalogMatches(command,view->query ? view->query : "")) xuiTreeViewSetSelected(app->commands,100+(int)index);
    xuiLabelSetText(app->helpTitle,command->name);
    xuiLabelSetText(app->helpDescription,*command->description ? command->description : "当前来源尚未提供功能说明；请结合签名与源文件查阅。");
    xuiLabelSetText(app->helpSignature,command->signature);
    char* params=parametersText(command); if(!params) return 0;
    xuiLabelSetText(app->helpParameters,params); free(params);
    size_t size=strlen(command->returns)+32; char* returns=malloc(size); if(!returns) return 0;
    snprintf(returns,size,"返回值：%s",command->returns); xuiLabelSetText(app->helpReturn,returns); free(returns);
    xuiCodeEditSetText(app->helpExample,command->example); xideConfigureEditorLanguage(app->helpExample,command->language);
    xuiLabelSetText(view->exampleHeading,*command->example ? "范例" : "当前来源尚未提供范例。");
    size=strlen(command->source)+strlen(command->version)+strlen(command->module)+strlen(command->requiredImport)+320;
    char* metadata=malloc(size); if(!metadata) return 0;
    snprintf(metadata,size,"模块：%s · 版本：%s\n导入要求：%s\n来源：%s:%d\n这是随程序提供的静态目录；实际可用 API 由所选编译器与项目决定。",command->module,command->version,*command->requiredImport ? command->requiredImport : "内置 / 语言结构",command->source,command->sourceLine);
    xuiLabelSetText(view->metadata,metadata); free(metadata); xuiTableViewRefreshAdapter(view->parameters);
    view->helpRetry=0;
    if(automatic){if(xideHelpSetDeferred(view->helpDocument,command->documents,command->documentCount,&app->catalog)!=XUI_OK){view->helpRetry=1;xuiWidgetSetVisible(xideHelpContent(view->helpDocument),0);}}
    else xideHelpSet(view->helpDocument,command->documents,command->documentCount);
    xideCatalogViewLayout(app);
    if(changed) xuiScrollViewSetOffset(app->help,0,0);
    xgeRenderRequest(); return 1;
}
int xideCatalogViewSelect(xide_app* app,size_t index) {return selectCommand(app,index,0);}
static void selected(xui_widget widget,int node,void* user)
{
    (void)widget; xide_app* app=user;
    if(app->catalogView && app->catalogView->publishing)return;
    if(node>=100 && (size_t)(node-100)<app->catalog.count) xideWorkbenchShowHelp(app,node-100);
}
int xideCatalogViewSearch(xide_app* app,const char* query)
{
    if(!app || !app->catalogView || !query) return XUI_ERROR_INVALID_ARGUMENT;
    xide_catalog_view* view=app->catalogView;
    if(strcmp(xuiEditGetText(view->search),query)) xuiEditSetText(view->search,query);
    if(view->query && !strcmp(view->query,query)) return XUI_OK;
    char* saved=malloc(strlen(query)+1); if(!saved) return XUI_ERROR_OUT_OF_MEMORY; strcpy(saved,query);
    /* Keep the query independent of Edit's storage while rebuilding its sibling tree. */
    if(!app->catalog.count && !view->settled){free(view->query);view->query=saved;return XUI_OK;}
    xui_tree_view_node_t* nodes=NULL;int nodeCount=0;size_t visible=0;
    int committed=xideCatalogTreePrepare(&app->catalog,saved,&nodes,&nodeCount,&visible);
    if(committed==XUI_OK){view->publishing=1;committed=xuiTreeViewSetNodes(app->commands,nodes,nodeCount);view->publishing=0;}
    free(nodes);if(committed!=XUI_OK){free(saved);return committed;}
    free(view->query);view->query=saved;view->visible=visible;
    if(view->selected && xideCatalogMatches(view->selected,saved)) xuiTreeViewSetSelected(app->commands,100+app->selectedCommand);
    char count[80]; snprintf(count,sizeof(count),"%zu / %zu 条命令",view->visible,app->catalog.count); xuiLabelSetText(view->count,count);
    xgeRenderRequest(); return XUI_OK;
}
static int searchEvent(xui_widget widget,const xui_edit_event_t* event,void* user)
{
    if(event->iType==XUI_EDIT_EVENT_TEXT_CHANGED) return xideCatalogViewSearch(user,xuiEditGetText(widget));
    return XUI_OK;
}
static int makeDetails(const xide_command_catalog* catalog,char*** out)
{
    char** details=calloc(catalog->count+1,sizeof(*details)); if(!details) return XUI_ERROR_OUT_OF_MEMORY;
    for(size_t i=0;i<catalog->count;++i) {
        const xide_command* command=&catalog->entries[i]; size_t size=strlen(command->signature)+strlen(command->requiredImport)+48;
        details[i]=malloc(size);
        if(!details[i]) { for(size_t j=0;j<i;++j) free(details[j]); free(details); return XUI_ERROR_OUT_OF_MEMORY; }
        snprintf(details[i],size,"%s%s%s",command->signature,*command->requiredImport ? " · 需 import " : "",command->requiredImport);
    }
    *out=details; return XUI_OK;
}
static void emptyHelp(xide_app* app)
{
    xide_catalog_view* view=app->catalogView;
    xuiLabelSetText(app->helpTitle,"命令目录不可用");
    xuiLabelSetText(app->helpDescription,"请修复或重新放入命令目录，然后点击左侧的“重新加载命令目录”。工作空间、文件编辑和构建运行仍可使用。");
    size_t size=strlen(view->catalogFile)+strlen(view->loadError)+48; char* detail=malloc(size);
    if(detail) { snprintf(detail,size,"目录路径：%s\n%s",view->catalogFile,view->loadError); xuiLabelSetText(view->metadata,detail); free(detail); }
    xui_widget hidden[]={app->helpSignature,app->helpParameters,view->parameters,app->helpReturn,view->exampleHeading,app->helpExample,view->insert};
    for(size_t i=0;i<sizeof(hidden)/sizeof(*hidden);++i) xuiWidgetSetVisible(hidden[i],0);
}
int xideCatalogViewReload(xide_app* app)
{
    if(!app || !app->catalogView)return 0;
    xide_catalog_view* view=app->catalogView;
    xide_catalog_job* job=xideCatalogJobStart(view->catalogFile);if(!job)return 0;
    xideCatalogJobCancel(view->load);view->load=job;view->settled=0;
    xuiLabelSetText(view->status,"命令列表加载中 ...");xuiWidgetSetVisible(view->status,1);
    if(!app->catalog.count){xui_tree_view_node_t node={.iId=0,.iParent=-1,.sText="【命令列表加载中 ...】"};xuiTreeViewSetNodes(app->commands,&node,1);}
    xgeRenderRequest();return 1;
}
int xideCatalogViewSettled(xide_app* app){return app && app->catalogView && app->catalogView->settled;}
void xideCatalogViewRestoreSelection(xide_app* app,const char* id)
{
    if(!app || !app->catalogView || !id)return;
    xide_catalog_view* v=app->catalogView;
    if(v->settled){const xide_command* c=xideCatalogLookupId(v->lookup,id);if(c)xideCatalogViewSelect(app,(size_t)(c-app->catalog.entries));return;}
    char* value=malloc(strlen(id)+1);if(!value)return;strcpy(value,id);free(v->deferredSelection);v->deferredSelection=value;
}
const char* xideCatalogViewSelectionId(xide_app* app)
{ xide_catalog_view* v=app?app->catalogView:NULL;return v && v->deferredSelection && !v->settled?v->deferredSelection:v && v->selected?v->selected->id:""; }
void xideCatalogViewLoadStats(xide_app* app,double* load,double* publish)
{if(load)*load=app->catalogView?app->catalogView->loadMs:0;if(publish)*publish=app->catalogView?app->catalogView->publishMs:0;}
static void publishCatalog(xide_app* app)
{
    xide_catalog_view* v=app->catalogView;xide_catalog_result result={0};
    if(!v->load || (app->startupAsync && !app->frames) || !xideCatalogJobTake(v->load,&result))return;
    LARGE_INTEGER begin,end,frequency;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
    v->settled=1;v->loadMs=result.loadMs;
    if(*result.error){snprintf(v->loadError,sizeof(v->loadError),"目录加载失败：%.500s",result.error);goto failed;}
    if(v->query && *v->query){free(result.nodes);result.nodes=NULL;if(xideCatalogTreePrepare(&result.catalog,v->query,&result.nodes,&result.nodeCount,&result.visible)!=XUI_OK)goto failed;}
    v->publishing=1;int status=xuiTreeViewSetNodes(app->commands,result.nodes,result.nodeCount);v->publishing=0;
    if(status!=XUI_OK){snprintf(v->loadError,sizeof(v->loadError),"无法交付命令树：%d",status);goto failed;}
    char* selectedId=NULL;const char* id=app->initialCommand?app->initialCommand:v->deferredSelection?v->deferredSelection:v->selected?v->selected->id:NULL;
    if(id){selectedId=malloc(strlen(id)+1);if(selectedId)strcpy(selectedId,id);}
    for(size_t i=0;i<app->documents.count;++i)if(app->documents.items[i]->editor)xuiCodeEditCancelCompletion(app->documents.items[i]->editor);
    v->pendingEditor=NULL;v->selected=NULL;xideHelpSet(v->helpDocument,NULL,0);followReset(app);
    for(size_t i=0;i<v->detailCount;++i)free(v->details[i]);
    free(v->details);xideCatalogLookupDestroy(v->lookup);xideCatalogClear(&app->catalog);
    app->catalog=result.catalog;memset(&result.catalog,0,sizeof(result.catalog));v->details=result.details;result.details=NULL;v->detailCount=app->catalog.count;v->lookup=result.lookup;result.lookup=NULL;v->visible=result.visible;
    const xide_command* command=selectedId?(app->initialCommand?xideCatalogFind(&app->catalog,selectedId):xideCatalogLookupId(v->lookup,selectedId)):NULL;free(selectedId);
    if(app->initialCommand && !command){xideFailureSet(&app->failure,XIDE_FAILURE_COMMAND,"XUI",XUI_ERROR_INVALID_ARGUMENT,app->initialCommand,"catalog startup selection");app->frameError=XUI_ERROR_INVALID_ARGUMENT;}
    app->initialCommand=NULL;free(v->deferredSelection);v->deferredSelection=NULL;
    if(app->catalog.count){size_t index=command?(size_t)(command-app->catalog.entries):0;selectCommand(app,index,1);v->publishing=1;xuiTreeViewSetSelected(app->commands,100+(int)index);v->publishing=0;}
    for(size_t i=0;i<app->documents.count;++i)if(app->documents.items[i]->open)xideCatalogBindEditor(app,app->documents.items[i]->editor);
    char count[80];snprintf(count,sizeof(count),"%zu / %zu 条命令",v->visible,app->catalog.count);xuiLabelSetText(v->count,count);v->loadError[0]=0;xuiWidgetSetVisible(v->status,0);goto done;
failed:
    if(app->initialCommand){xideFailureSet(&app->failure,XIDE_FAILURE_COMMAND,"XUI",XUI_ERROR,app->initialCommand,"catalog loading failed");app->initialCommand=NULL;app->frameError=XUI_ERROR;}
    if(!*v->loadError)snprintf(v->loadError,sizeof(v->loadError),"命令目录准备失败；保留上一份可用目录。");
    xuiLabelSetText(v->status,v->loadError);xuiWidgetSetVisible(v->status,1);
    if(!app->catalog.count){xui_tree_view_node_t node={.iId=0,.iParent=-1,.sText="命令列表加载失败"};xuiTreeViewSetNodes(app->commands,&node,1);emptyHelp(app);}
done:
    xideCatalogResultClear(&result);xideCatalogJobCancel(v->load);v->load=NULL;QueryPerformanceCounter(&end);v->publishMs=(double)(end.QuadPart-begin.QuadPart)*1000/frequency.QuadPart;xgeRenderRequest();
}
static void reloadClicked(xui_widget widget,void* user) { (void)widget; xideCatalogViewReload(user); }
int xideCatalogViewCreate(xide_app* app)
{
    xide_catalog_view* view=calloc(1,sizeof(*view)); if(!view) return XUI_ERROR_OUT_OF_MEMORY; app->catalogView=view;
    wchar_t binary[32768]; if(!GetModuleFileNameW(NULL,binary,32768)) return XUI_ERROR;
    const wchar_t* resource=xideStartupResourceModule(binary);
    int size=WideCharToMultiByte(CP_UTF8,0,resource,-1,NULL,0,NULL,NULL); char* path=size ? malloc((size_t)size) : NULL;
    if(!path) return XUI_ERROR_OUT_OF_MEMORY;
    WideCharToMultiByte(CP_UTF8,0,resource,-1,path,size,NULL,NULL); char* base=xidePathDirectory(path); free(path);
    view->catalogFile=app->catalogPath ? xidePathResolve(NULL,app->catalogPath) : base ? xidePathResolve(base,"data/commands.json") : NULL; free(base);
    if(!view->catalogFile) return XUI_ERROR_OUT_OF_MEMORY;
    xide_model_error error={0};
    (void)error;view->settled=app->catalog.count!=0;
    TRY(makeDetails(&app->catalog,&view->details)); view->detailCount=app->catalog.count;
    view->lookup=xideCatalogLookupCreate(&app->catalog);if(!view->lookup)return XUI_ERROR_OUT_OF_MEMORY;
    TRY(xuiWidgetCreate(app->context,&view->panel)); TRY(xuiWidgetSetLayoutType(view->panel,XUI_LAYOUT_COLUMN)); TRY(xuiWidgetSetPadding(view->panel,(xui_thickness_t){5,5,5,5})); TRY(xuiWidgetSetGap(view->panel,4));
    xui_input_desc_t edit={0}; edit.iSize=sizeof(edit); edit.pFont=app->uiFont; edit.sText="";
    TRY(xuiInputCreate(app->context,&view->search,&edit)); TRY(xuiEditSetEvent(view->search,searchEvent,app)); TRY(xuiInputSetPlaceholder(view->search,"搜索命令…"));
    TRY(label(app,&view->count,"检索名称、模块或签名",1));
    TRY(label(app,&view->status,view->loadError,1)); TRY(ideThemeSetTextRole(view->status,IDE_WARNING));
    xui_button_desc_t reload={0}; reload.iSize=sizeof(reload); reload.pFont=app->uiFont; reload.sText="重新加载命令目录";
    TRY(xuiButtonCreate(app->context,&view->reload,&reload)); TRY(xuiButtonSetClick(view->reload,reloadClicked,app));
    xui_tree_view_desc_t tree={0}; tree.iSize=sizeof(tree); tree.pFont=app->uiFont; tree.fItemHeight=26; tree.fIndent=12; tree.fPadding=4;
    TRY(xuiTreeViewCreate(app->context,&app->commands,&tree)); TRY(xuiTreeViewSetSelect(app->commands,selected,app));
    xui_widget left[]={view->search,view->status,view->count,app->commands,view->reload};
    for(int i=0;i<5;++i) { TRY(xuiWidgetSetSizeMode(left[i],XUI_SIZE_FILL,i==3 ? XUI_SIZE_FILL : XUI_SIZE_FIXED)); if(i!=3) TRY(xuiWidgetSetPreferredSize(left[i],(xui_vec2_t){0,i==1 ? 120 : i==2 ? 24 : 30})); TRY(xuiWidgetAddChild(view->panel,left[i])); }
    app->panels[XIDE_COMMANDS]=view->panel;
    xui_scroll_view_desc_t scroll={0}; scroll.iSize=sizeof(scroll); scroll.fContentWidth=400; scroll.fContentHeight=500; scroll.iPolicyX=XUI_SCROLLBAR_POLICY_HIDDEN; scroll.iPolicyY=XUI_SCROLLBAR_POLICY_AUTO; scroll.iScrollbarMode=XUI_SCROLLBAR_MODE_COMPACT; scroll.fScrollbarSize=10; scroll.fWheelStep=42;
    TRY(xuiScrollViewCreate(app->context,&app->help,&scroll)); app->helpContent=xuiScrollViewGetContentWidget(app->help);
    TRY(xuiWidgetSetLayoutType(app->helpContent,XUI_LAYOUT_COLUMN)); TRY(xuiWidgetSetPadding(app->helpContent,(xui_thickness_t){12,12,12,12})); TRY(xuiWidgetSetGap(app->helpContent,8));
    TRY(label(app,&app->helpTitle,"集成帮助",0)); TRY(label(app,&app->helpDescription,"",0)); TRY(label(app,&app->helpSignature,"",1)); TRY(label(app,&app->helpParameters,"",1)); TRY(label(app,&app->helpReturn,"",1)); TRY(label(app,&view->exampleHeading,"范例",0)); TRY(label(app,&view->metadata,"",1));
    xui_table_view_desc_t table={0}; table.iSize=sizeof(table); table.pFont=app->uiFont; table.fDefaultRowHeight=28; table.fHeaderHeight=28; table.onCount=parameterCount; table.onCell=parameterCell; table.pAdapterUser=view;
    TRY(xuiTableViewCreate(app->context,&view->parameters,&table));
    xui_code_edit_desc_t code={0}; code.iSize=sizeof(code); code.pFont=app->codeFont; code.sText=""; code.bReadonly=1; code.bShowLineNumbers=1; code.bShowFoldMargin=1; code.iTabColumns=4; code.iIndentColumns=4;
    TRY(xideDpiCodeEditCreate(app->context,&app->helpExample,&code)); TRY(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(app->helpExample))); TRY(xideConfigureEditorLanguage(app->helpExample,"xlang"));
    xui_button_desc_t button={0}; button.iSize=sizeof(button); button.pFont=app->uiFont; button.sText="插入范例到当前文件";
    TRY(xuiButtonCreate(app->context,&view->insert,&button)); TRY(xuiButtonSetClick(view->insert,insertClicked,app));
    button.sText="查看声明来源"; TRY(xuiButtonCreate(app->context,&view->sourceButtons[0],&button)); TRY(xideActionsButton(app,view->sourceButtons[0],XIDE_ACTION_SOURCE_DECLARATION));
    button.sText="查看文档来源"; TRY(xuiButtonCreate(app->context,&view->sourceButtons[1],&button)); TRY(xideActionsButton(app,view->sourceButtons[1],XIDE_ACTION_SOURCE_DOCUMENT));
    button.sText="返回引用编辑（草稿已保留）"; TRY(xuiButtonCreate(app->context,&view->returnReferences,&button)); TRY(xuiButtonSetClick(view->returnReferences,returnReferencesClicked,app));
    TRY(xuiWidgetSetVisible(view->returnReferences,0)); TRY(xideHelpCreate(app,&view->helpDocument));
    TRY(xuiWidgetCreate(app->context,&view->helpPanel)); TRY(xuiWidgetSetLayoutType(view->helpPanel,XUI_LAYOUT_COLUMN)); TRY(xuiWidgetSetGap(view->helpPanel,4));
    TRY(followCreate(app));
    xui_widget helpChildren[]={view->returnReferences,xideHelpPages(view->helpDocument),xideHelpNavigator(view->helpDocument),app->help};
    for(int i=0;i<4;++i) { TRY(xuiWidgetSetSizeMode(helpChildren[i],XUI_SIZE_FILL,i==3 ? XUI_SIZE_FILL : XUI_SIZE_FIXED)); if(i<3) TRY(xuiWidgetSetPreferredSize(helpChildren[i],(xui_vec2_t){0,30})); TRY(xuiWidgetAddChild(view->helpPanel,helpChildren[i])); }
    app->panels[XIDE_HELP]=view->helpPanel;
    /* Keep the new controls in the reading viewport: fixed tool rows can
       otherwise consume an entire short Dock pane at high DPI. */
    xui_widget children[]={view->followBar,view->followChoices,app->helpTitle,app->helpDescription,app->helpSignature,app->helpParameters,view->parameters,app->helpReturn,view->exampleHeading,app->helpExample,view->insert,xideHelpContent(view->helpDocument),view->metadata,view->sourceButtons[0],view->sourceButtons[1]};
    for(size_t i=0;i<sizeof(children)/sizeof(*children);++i) { TRY(xuiWidgetSetSizeMode(children[i],XUI_SIZE_FILL,XUI_SIZE_FIXED)); TRY(xuiWidgetAddChild(app->helpContent,children[i])); }
    xuiWidgetSetVisible(view->status,*view->loadError!=0);
    if(!app->catalog.count){xuiLabelSetText(app->helpTitle,"集成帮助");xuiLabelSetText(app->helpDescription,"命令列表加载中 ...；可继续打开文件和编辑。");}
    xideHelpSet(view->helpDocument,NULL,0);
    TRY(xideCatalogViewSearch(app,""));
    if(!app->catalog.count && !xideCatalogViewReload(app))return XUI_ERROR_OUT_OF_MEMORY;
    return XUI_OK;
}
static int documentRow(xide_app* app)
{
 xui_font_metrics_t m={0};float row=28;
 if(app->documentFont && app->proxy.fontGetMetrics(&app->proxy,app->documentFont,&m)==XUI_OK)row=ceilf(m.fLineHeight)+xideSettingsOptions(&app->preferences)->documentLineGap+8;
 return row<28 ? 28 : (int)row;
}
void xideCatalogViewLayout(xide_app* app)
{
    xide_catalog_view* view=app->catalogView; if(!view) return;
    sourceLayout(app);
    xuiWidgetSetPreferredSize(view->followBar,(xui_vec2_t){0,xideUiHeight(app,28)});
    xuiWidgetSetPreferredSize(view->followChoices,(xui_vec2_t){0,xideUiHeight(app,30)});
    xuiWidgetSetPreferredSize(view->followPin,(xui_vec2_t){xideUiHeight(app,156),xideUiHeight(app,28)});
    xui_widget choicesPopup=xuiMenuGetPopupWidget(xuiComboBoxGetMenuWidget(view->followChoices));
    if(choicesPopup)xuiPopupSetMaxSize(choicesPopup,fmaxf(1,app->viewportWidth-32),fmaxf(1,app->viewportHeight-32));
    for(int i=0;i<2;++i) { xuiWidgetSetVisible(view->sourceButtons[i],view->selected!=NULL && (i==0 || view->selected->documentCount>0)); xuiWidgetSetPreferredSize(view->sourceButtons[i],(xui_vec2_t){0,xideUiHeight(app,32)}); }
    xuiWidgetSetVisible(view->returnReferences,xideReferencesViewSuspended(app));
    xuiWidgetSetPreferredSize(view->returnReferences,(xui_vec2_t){0,(float)xideUiHeight(app,32)});
    xui_rect_t leftRect=xuiWidgetGetContentRect(view->panel);
    xuiWidgetSetPreferredSize(view->search,(xui_vec2_t){0,(float)xideUiHeight(app,30)});
    xuiWidgetSetPreferredSize(view->count,(xui_vec2_t){0,(float)xideUiHeight(app,24)});
    xuiWidgetSetPreferredSize(view->reload,(xui_vec2_t){0,(float)xideUiHeight(app,30)});
    xuiWidgetSetPreferredSize(xideHelpPages(view->helpDocument),(xui_vec2_t){0,(float)xideUiHeight(app,30)});
    xuiWidgetSetPreferredSize(xideHelpNavigator(view->helpDocument),(xui_vec2_t){0,(float)xideUiHeight(app,30)});
    if(xuiWidgetGetVisible(view->status)) { xui_vec2_t measured={0}; xuiWidgetMeasureContent(view->status,(xui_vec2_t){leftRect.fW-10,XUI_LAYOUT_UNBOUNDED},&measured); xuiWidgetSetPreferredSize(view->status,(xui_vec2_t){0,measured.fY>24 ? measured.fY+8 : 32}); }
    if(!view->selected) {
        xui_rect_t rect=xuiWidgetGetContentRect(app->help); int width=rect.fW>180 ? rect.fW-14 : 180,total=24+xideUiHeight(app,28)+8+(xuiWidgetGetVisible(view->followChoices) ? xideUiHeight(app,30)+8:0);
        xui_widget labels[]={app->helpTitle,app->helpDescription,view->metadata};
        for(size_t i=0;i<3;++i) { xui_vec2_t measured={0}; xuiWidgetMeasureContent(labels[i],(xui_vec2_t){width-24,XUI_LAYOUT_UNBOUNDED},&measured); int height=measured.fY+8; if(height<28) height=28; xuiWidgetSetPreferredSize(labels[i],(xui_vec2_t){0,height}); total+=height+8; }
        xuiScrollViewSetContentSize(app->help,width,total>rect.fH ? total : rect.fH); return;
    }
    const xide_command* command=view->selected; xui_rect_t rect=xuiWidgetGetContentRect(app->help);
    int width=rect.fW-14; if(width<160) width=160;
    if(!xideSettingsOptions(&app->preferences)->documentWrap){
        xui_widget text[]={app->helpTitle,app->helpDescription,app->helpParameters,app->helpReturn};
        for(size_t i=0;i<sizeof(text)/sizeof(*text);++i){xui_vec2_t size={0};if(xuiWidgetMeasureContent(text[i],(xui_vec2_t){XUI_LAYOUT_UNBOUNDED,XUI_LAYOUT_UNBOUNDED},&size)==XUI_OK && size.fX+24>width)width=(int)ceilf(size.fX)+24;}
        width=xideHelpDocumentWidth(view->helpDocument,width-24)+24;
    }
    int table=width>=600 && command->parameterCount>0;
    xuiWidgetSetVisible(view->parameters,table); xuiWidgetSetVisible(app->helpParameters,command->parameterCount>0);
    xuiWidgetSetVisible(app->helpReturn,*command->returns!=0); xuiWidgetSetVisible(app->helpExample,*command->example!=0); xuiWidgetSetVisible(view->insert,*command->example!=0); xuiWidgetSetEnabled(view->insert,validTarget(app));
    xuiWidgetSetVisible(app->helpSignature,1); xuiWidgetSetVisible(view->exampleHeading,1);
    if(table) {
        xuiLabelSetText(app->helpParameters,"参数");
        if(view->tableWidth!=width) {
            view->tableWidth=width; const char* titles[]={"名称","类型","要求","默认值","说明"}; float sizes[]={0.19f,0.21f,0.10f,0.20f,0.30f}; xui_table_view_column_t columns[5]={0};
            for(int i=0;i<5;++i) { columns[i].iId=i; columns[i].sTitle=titles[i]; columns[i].fWidth=(width-24)*sizes[i]; columns[i].fMinWidth=32; columns[i].bResizable=1; columns[i].bResizableSet=1; }
            xuiTableViewSetColumns(view->parameters,columns,5);
        }
    } else if(command->parameterCount && view->tableMode!=table) { char* params=parametersText(command); if(params) { xuiLabelSetText(app->helpParameters,params); free(params); } }
    view->tableMode=table;
    xui_widget labels[]={app->helpTitle,app->helpDescription,app->helpSignature,app->helpParameters,app->helpReturn,view->exampleHeading,view->metadata};
    int total=24+xideUiHeight(app,28)+8+(xuiWidgetGetVisible(view->followChoices) ? xideUiHeight(app,30)+8:0);
    for(size_t i=0;i<sizeof(labels)/sizeof(*labels);++i) if(xuiWidgetGetVisible(labels[i])) {
        xui_vec2_t measured={0}; xuiWidgetMeasureContent(labels[i],(xui_vec2_t){width-24,XUI_LAYOUT_UNBOUNDED},&measured);
        int height=(int)measured.fY+8; if(height<28) height=28; xuiWidgetSetPreferredSize(labels[i],(xui_vec2_t){0,height}); total+=height+8;
    }
    if(table) { int height=documentRow(app)*(1+(int)command->parameterCount)+4; xuiWidgetSetPreferredSize(view->parameters,(xui_vec2_t){0,height}); total+=height+8; }
    if(*command->example) {
        int lines=1; for(const char* p=command->example;*p;++p) if(*p=='\n') ++lines;
        int height=(int)ceilf(lines*xideEditorLineHeight(app))+24; if(height<100) height=100;
        xuiWidgetSetPreferredSize(app->helpExample,(xui_vec2_t){0,height}); xuiWidgetSetPreferredSize(view->insert,(xui_vec2_t){0,(float)xideUiHeight(app,32)}); total+=height+xideUiHeight(app,32)+16;
    }
    int documentHeight=xideHelpLayout(view->helpDocument,width-24); if(documentHeight) total+=documentHeight+8;
    for(int i=0;i<2;++i) if(xuiWidgetGetVisible(view->sourceButtons[i])) total+=xideUiHeight(app,32)+8;
    xuiScrollViewSetContentSize(app->help,width,total>rect.fH ? total : rect.fH);
}
void xideCatalogViewDestroy(xide_app* app)
{
    xide_catalog_view* view=app->catalogView; if(!view) return;
    if(view->details) for(size_t i=0;i<view->detailCount;++i) free(view->details[i]);
    xideCatalogJobCancel(view->load);free(view->deferredSelection);
    followDestroy(view);xideCatalogLookupDestroy(view->lookup);
    xideHelpDestroy(view->helpDocument); free(view->catalogFile); free(view->details); free(view->query); free(view); app->catalogView=NULL;
}
xui_widget xideCatalogSearchWidget(xide_app* app) { return app && app->catalogView ? app->catalogView->search : NULL; }
xui_widget xideCatalogInsertWidget(xide_app* app) { return app && app->catalogView ? app->catalogView->insert : NULL; }
size_t xideCatalogVisibleCount(xide_app* app) { return app && app->catalogView ? app->catalogView->visible : 0; }
static int identifier(unsigned char c) { return c>=128 || isalnum(c) || c=='_' || c=='.'; }
static int complete(xui_widget editor,int offset,const char* prefix,xui_code_completion_item_t* items,int capacity,int* count,void* user)
{
    (void)prefix; xide_app* app=user; if(!count) return XUI_ERROR_INVALID_ARGUMENT; *count=0;
    /* The matched SDK validates the number of initialized output slots. */
    if(!items || capacity<=0) return XUI_OK;
    const char* text=xuiCodeEditGetText(editor); xui_code_document document=xuiCodeEditGetDocument(editor);
    if(!text || offset<0 || offset>xuiCodeDocumentGetLength(document) || xuiCodeEditIsReadonly(editor) || !app->catalogView) return XUI_OK;
    int open=0; for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->editor==editor && app->documents.items[i]->open) open=1;
    if(!open) return XUI_OK;
    if(offset>0) {
        xui_code_token_t tokens[8]; int tokenCount=0;
        if(xuiCodeTokenBufferGetTokensInRange(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(document),offset-1,offset,tokens,8,&tokenCount)!=XUI_OK) return XUI_OK;
        for(int i=0;i<tokenCount && i<8;++i) if(tokens[i].iKind==XUI_CODE_TOKEN_COMMENT || tokens[i].iKind==XUI_CODE_TOKEN_STRING || tokens[i].iKind==XUI_CODE_TOKEN_CHAR || tokens[i].iKind==XUI_CODE_TOKEN_ERROR) return XUI_OK;
    }
    int start=offset; while(start>0 && identifier((unsigned char)text[start-1])) --start;
    size_t length=(size_t)(offset-start),qualifier=0;
    for(size_t i=0;i<length;++i) if(text[start+(int)i]=='.') qualifier=i+1;
    xideCompletionViewCollect(app,editor,offset,items,capacity,count);

#if XIDE_ENABLE_C_LSP
 xideCCompletionViewCollect(app,editor,offset,items,capacity,count);
#endif

    const char* language=xuiCodeEditGetLanguage(editor); const char* last=NULL;
    for(size_t i=0;i<app->catalog.count;++i) {
        const xide_command* command=&app->catalog.entries[i]; size_t nameLength=strlen(command->name);
        /* Bound catalog candidates by the actual qualified prefix before the
           SDK's fuzzy ranking. Semantic overloads remain distinct above. */
        if(!xideCatalogCanComplete(command,language) || nameLength<length || (length && strncmp(command->name,text+start,length)) || (last && !strcmp(last,command->name))) continue;
        /* Qualified insertion replaces only CodeEdit's final identifier prefix. */
        if(qualifier && nameLength<=qualifier) continue;
        const char* insert=command->name+qualifier; last=command->name;
        if(xideCompletionViewHasName(app,editor,insert)
#if XIDE_ENABLE_C_LSP
 || xideCCompletionViewHasName(app,editor,insert)
#endif
) continue;
        if(items && *count<capacity) {
            xui_code_completion_item_t item={0}; item.iSize=sizeof(item); item.sLabel=insert; item.sInsertText=insert; item.sFilterText=insert;
            item.sDetail=app->catalogView->details[i]; item.sDocumentation=command->description; item.iUserData=i; item.iSortOrder=(int)i;
            items[*count]=item;
        }
        if(*count<capacity || !items) ++*count;
    }
    return XUI_OK;
}
static int input(xui_widget editor,const xui_code_input_event_t* event,xui_code_input_action_t* action,void* user)
{
    xide_app* app=user; xideCompletionViewInput(app,editor,event,action);
#if XIDE_ENABLE_C_LSP
 xideCCompletionViewInput(app,editor,event,action);
#endif

    if(!event || !xideSettingsOptions(&app->preferences)->completion || !app->catalogView || event->iPhase!=XUI_CODE_INPUT_COMMITTED || event->iSource==XUI_CODE_INPUT_SOURCE_PASTE || event->iSource==XUI_CODE_INPUT_SOURCE_COMPLETION || event->iTextSize!=1 || !event->sText) return XUI_OK;
    int offset=event->tSelectionAfter.iCaretOffset;
    const char* text=xuiCodeEditGetText(editor);
    if(event->sText[0]!='.' && !(event->sText[0]=='>' && !strcmp(xuiCodeEditGetLanguage(editor),"c") && offset>=2 && text[offset-2]=='-')) return XUI_OK;
    for(size_t i=0;i<app->documents.count;++i) if(app->documents.items[i]->editor==editor && app->documents.items[i]->open) {
        xide_catalog_view* view=app->catalogView; view->pendingEditor=editor; view->pendingDocument=app->documents.items[i]->id;
        view->pendingVersion=xuiCodeDocumentGetVersion(event->pDocument); view->pendingOffset=event->tSelectionAfter.iCaretOffset;
        snprintf(view->pendingLanguage,sizeof(view->pendingLanguage),"%s",xuiCodeEditGetLanguage(editor)); break;
    }
    return XUI_OK;
}
void xideCatalogViewPoll(xide_app* app)
{
    xide_catalog_view* view=app->catalogView;if(!view)return;
    publishCatalog(app);
    if(view->load)xgeRenderRequestAfter(0.016f);
    followPoll(app,GetTickCount64(),0);if(helpVisible(app)){if(view->helpRetry && view->selected && xideHelpSetDeferred(view->helpDocument,view->selected->documents,view->selected->documentCount,&app->catalog)==XUI_OK)view->helpRetry=0;xideHelpPoll(view->helpDocument);}
    if(!view->pendingEditor)return;
    if(!xideSettingsOptions(&app->preferences)->completion){view->pendingEditor=NULL;return;}
    xui_widget editor=view->pendingEditor; view->pendingEditor=NULL;
    xide_document* document=app->activeDocument;
    if(!document || !document->open || document->editor!=editor || document->id!=view->pendingDocument || xuiGetFocusWidget(app->context)!=editor || strcmp(view->pendingLanguage,xuiCodeEditGetLanguage(editor))) return;
    xui_code_selection_t selection={0}; xuiCodeSelectionGetState(xuiCodeEditGetSelection(editor),&selection);
    if(view->pendingVersion!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)) || selection.iCaretOffset!=view->pendingOffset || selection.iAnchorOffset!=selection.iCaretOffset) return;
    xuiCodeEditShowCompletion(editor);
}
int xideCatalogBindEditor(xide_app* app,xui_widget editor)
{
    TRY(followBind(app,editor));
    /* SHOW_COMPLETION is followed by generic command cancellation in this
       SDK. The registered IDE hotkey calls ShowCompletion exactly once. */
    int ret=xuiCodeCommandMapUnbind(xuiCodeEditGetCommandMap(editor),XUI_KEY_SPACE,XUI_MOD_CTRL);
    if(ret!=XUI_OK && ret!=XUI_ERROR_UNSUPPORTED) return ret;
    ret=xuiCodeCommandMapUnbind(xuiCodeEditGetCommandMap(editor),XUI_KEY_SPACE,XUI_MOD_CTRL|XUI_MOD_SHIFT);
    if(ret!=XUI_OK && ret!=XUI_ERROR_UNSUPPORTED)return ret;
    TRY(xuiCodeProviderSetCompletion(xuiCodeEditGetProviders(editor),complete,app));
    TRY(xuiCodeEditSetInputHandler(editor,input,app)); return xuiCodeEditSetCompletionOptions(editor,xideSettingsOptions(&app->preferences)->completion,1,app->catalog.count+4096<16384 ? (int)app->catalog.count+4096 : 16384,0.08f);
}
xide_help_view* xideCatalogHelpDocument(xide_app* app) { return app && app->catalogView ? app->catalogView->helpDocument : NULL; }
void xideCatalogViewAfterLayout(xide_app* app) { if(app && app->catalogView) xideHelpApplyJump(app->catalogView->helpDocument); }
int xideCatalogViewPrepareRender(xide_app* app)
{
    if(!app || !app->catalogView) return XUI_OK;
    TRY(xuiLayout(app->context));
    return xideHelpPrepareRender(app->catalogView->helpDocument);
}
int xideCatalogViewCodeFont(xide_app* app)
{
    if(app->helpSignature)TRY(xuiLabelSetFont(app->helpSignature,app->codeFont));
    if(!app || !app->catalogView) return XUI_OK;
    TRY(xideCodeWidgetSetFont(app,app->helpExample,app->codeFont));
    if(app->catalogView->sourceEditor) TRY(xideCodeWidgetSetFont(app,app->catalogView->sourceEditor,app->codeFont));
    return xideHelpCodeFont(app->catalogView->helpDocument);
}
int xideCatalogViewTheme(xide_app* app) { if(app && app->catalogView) { TRY(sourceTheme(app)); return xideHelpTheme(app->catalogView->helpDocument); } return XUI_OK; }

xui_widget xideCatalogReloadWidget(xide_app* app) { return app && app->catalogView ? app->catalogView->reload : NULL; }
xui_widget xideCatalogStatusWidget(xide_app* app) { return app && app->catalogView ? app->catalogView->status : NULL; }

int xideCatalogViewUiFont(xide_app* app)
{ return app->catalogView ? xideHelpUiFont(app->catalogView->helpDocument) : XUI_OK; }

char* xideCatalogTreeKey(xide_app* app,int id)
{
    const xui_tree_view_node_t* node=xuiTreeViewGetNodeById(app->commands,id); if(!node) return NULL;
    static const char* categories[]={"","language","standard","extension"};
    if(node->iParent<0 && id>=1 && id<=3) return xideReadingKey("category",categories[id],"");
    if(node->iParent>=1 && node->iParent<=3) return xideReadingKey("module",categories[node->iParent],node->sText);
    if(id>=100 && (size_t)(id-100)<app->catalog.count) return xideReadingKey("command","",app->catalog.entries[id-100].id);
    return NULL;
}

int xideCatalogResetReading(xide_app* app)
{
    if(!app->catalogView) return XUI_OK;
    free(app->catalogView->deferredSelection);app->catalogView->deferredSelection=NULL;
    free(app->catalogView->query); app->catalogView->query=NULL;
    int result=xideCatalogViewSearch(app,""); if(result!=XUI_OK) return result;
    if(app->catalog.count) xideCatalogViewSelect(app,0);
    xideHelpResetReading(app->catalogView->helpDocument); return xuiScrollViewSetOffset(app->help,0,0);
}

xui_widget xideCatalogReturnReferencesWidget(xide_app* app) { return app && app->catalogView ? app->catalogView->returnReferences : NULL; }

int xideCatalogViewDocumentFont(xide_app* app)
{
 if(!app->catalogView || !app->documentFont)return XUI_OK;
 xui_widget labels[]={app->helpTitle,app->helpDescription,app->helpParameters,app->helpReturn};
 const xide_settings_options* o=xideSettingsOptions(&app->preferences);
 for(size_t i=0;i<sizeof(labels)/sizeof(*labels);++i){TRY(xuiLabelSetFont(labels[i],app->documentFont));TRY(xuiLabelSetLineGap(labels[i],(float)o->documentLineGap));TRY(xuiLabelSetWrapMode(labels[i],o->documentWrap ? XUI_TEXT_WRAP_WORD : XUI_TEXT_WRAP_NONE));}
 TRY(xuiLabelSetFont(app->helpSignature,app->codeFont));TRY(xuiTableViewSetFont(app->catalogView->parameters,app->documentFont));
 float width=0,row=0,header=0;TRY(xuiTableViewGetDefaultMetrics(app->catalogView->parameters,&width,&row,&header));TRY(xuiTableViewSetDefaultMetrics(app->catalogView->parameters,width,(float)documentRow(app),(float)documentRow(app)));
 return xideHelpDocumentFont(app->catalogView->helpDocument);
}
