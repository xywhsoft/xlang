#include "ui/toolchain_view.h"
#include "platform/dpi_proxy.h"
#include "ui/resource_view.h"
#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "ui/references_view.h"
#include "ui/file_changes_view.h"
#include "services/file_sync.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "ui/font_view.h"
#include "ui/configuration_view.h"
#include "ui/recovery_view.h"
#include "ui/workspace_view.h"
#include "platform/path.h"
#include "lang/languages.h"
#include "theme/theme.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) return result_; } while(0)
struct xide_file_changes_view {
    xui_widget window,content,path,message,warning,controls[XIDE_CHANGES_CONTROL_COUNT]; xide_ui_layout layout;
    char** labels; uint64_t* tokens; size_t count; uint64_t revision,token,sequence,localSignature; uint32_t documentVersion;
    int updating,confirm,valid,cw,ch; float scale; char messageText[768];
};
static void message(xide_file_changes_view* v,const char* s) { snprintf(v->messageText,sizeof(v->messageText),"%s",s); xuiLabelSetText(v->message,v->messageText); }
static uint64_t hash(uint64_t h,const void* p,size_t n) { const unsigned char* s=p; for(size_t i=0;i<n;++i) { h^=s[i]; h*=UINT64_C(1099511628211); } return h; }
static const xide_file_change* selected(xide_app* app)
{ xide_file_changes_view* v=app->fileChangesView; for(size_t i=0;i<xideFileSyncCount(app);++i) { const xide_file_change* c=xideFileSyncAt(app,i); if(c && c->snapshot.token==v->token) return c; } return NULL; }
static xide_document* document(xide_app* app,const xide_file_change* c)
{ if(c) for(size_t i=0;i<app->documents.count;++i) { xide_document* d=app->documents.items[i]; if(d->open && d->id==c->documentId) return d; } return NULL; }
static xvalue* model(xide_app* app,const char* path,xide_model_error* e)
{ if(app->workspaceModel.manifest && xidePathEqual(app->workspaceModel.manifest,path)) return xideWorkspaceExport(&app->workspaceModel,e); for(size_t i=0;i<app->workspaceModel.projectCount;++i) if(xidePathEqual(app->workspaceModel.projects[i].manifest,path)) return xideProjectExport(&app->workspaceModel.projects[i],e); return NULL; }
static uint64_t localSignature(xide_app* app,const xide_file_change* c)
{
    uint64_t h=UINT64_C(14695981039346656037); xide_document* d=document(app,c);
    if(d) { uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)); h=hash(h,&version,sizeof(version)); }
    xide_model_error e={0}; xvalue* root=model(app,c->snapshot.path,&e); size_t n=0; char* text=root ? xrtJsonStringify(root,false,&n) : NULL;
    if(text) h=hash(h,text,n); else if(root) h=0; xrtFree(text); xrtValueRelease(root);
    if(app->workspaceModel.manifest && xidePathEqual(app->workspaceModel.manifest,c->snapshot.path)) for(size_t i=0;i<app->workspaceModel.projectCount;++i) {
        xide_project* p=&app->workspaceModel.projects[i]; root=p->available ? xideProjectExport(p,&e) : NULL; text=root ? xrtJsonStringify(root,false,&n) : NULL;
        if(text) h=hash(h,text,n); else if(root) h=0; xrtFree(text); xrtValueRelease(root);
    }
    root=xideWorkspaceViewDraftSnapshot(app); text=root ? xrtJsonStringify(root,false,&n) : NULL;
    if(text) h=hash(h,text,n); else if(root) h=0; xrtFree(text); xrtValueRelease(root);
    int draft=xideWorkspaceViewHasDraft(app); return h ? hash(h,&draft,sizeof(draft)) : 0;
}
static void enabled(xide_app* app)
{
    xide_file_changes_view* v=app->fileChangesView; const xide_file_change* c=selected(app); int valid=c && v->valid && c->snapshot.sequence==v->sequence;
    xuiWidgetSetEnabled(v->controls[XIDE_CHANGES_LIST],!v->confirm); xuiWidgetSetEnabled(v->controls[XIDE_CHANGES_KEEP],c && !v->confirm); xuiWidgetSetEnabled(v->controls[XIDE_CHANGES_RELOAD],valid && !v->confirm);
    xuiWidgetSetEnabled(v->controls[XIDE_CHANGES_REFRESH],!v->confirm); xuiWidgetSetVisible(v->warning,v->confirm);
    xuiWidgetSetVisible(v->controls[XIDE_CHANGES_CONFIRM],v->confirm); xuiWidgetSetVisible(v->controls[XIDE_CHANGES_CANCEL],v->confirm);
    xuiWidgetSetEnabled(v->controls[XIDE_CHANGES_CONFIRM],valid && v->confirm); xuiWidgetSetEnabled(v->controls[XIDE_CHANGES_CANCEL],v->confirm);
    const int buttons[]={XIDE_CHANGES_REFRESH,XIDE_CHANGES_KEEP,XIDE_CHANGES_RELOAD,XIDE_CHANGES_CLOSE,XIDE_CHANGES_CONFIRM,XIDE_CHANGES_CANCEL}; const uint32_t states[]={XUI_WIDGET_STATE_DISABLED,XUI_WIDGET_STATE_ACTIVE,XUI_WIDGET_STATE_HOVER,XUI_WIDGET_STATE_FOCUS};
    for(size_t i=0;i<sizeof(buttons)/sizeof(*buttons);++i) { xui_widget w=v->controls[buttons[i]]; uint32_t input=xuiWidgetGetInputState(w),state=0; for(size_t j=0;j<sizeof(states)/sizeof(*states);++j) if(input&states[j]) { state=states[j]; break; } xuiWidgetSetStateId(w,state); }
}
static void preview(xide_app* app)
{
    xide_file_changes_view* v=app->fileChangesView; const xide_file_change* c=selected(app); v->confirm=v->valid=0; v->sequence=v->localSignature=0; v->documentVersion=0;
    xuiCodeEditSetText(v->controls[XIDE_CHANGES_LOCAL],""); xuiCodeEditSetText(v->controls[XIDE_CHANGES_DISK],""); xuiEditSetText(v->path,c ? c->snapshot.path : "");
    if(c) {
        xide_document* d=document(app,c); const char* language=d ? d->language : "text"; xide_model_error e={0}; xvalue* root=model(app,c->snapshot.path,&e); size_t n=0; char* text=root ? xrtJsonStringify(root,true,&n) : NULL;
        const char* local=d ? xuiCodeEditGetText(d->editor) : text ? text : "当前没有可读取的工程内容。";
        if(d) v->documentVersion=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
        xvalue* draft=root ? xideWorkspaceViewDraftSnapshot(app) : NULL; size_t draftLength=0; char* draftText=draft ? xrtJsonStringify(draft,true,&draftLength) : NULL; char* combined=NULL;
        if(root && text) {
            size_t need=strlen(local)+strlen(text)+(draftText ? strlen(draftText) : 0)+192;
            combined=malloc(need);
            if(combined) {
                if(d) snprintf(combined,need,"当前文档：\n%s\n\n当前工程模型：\n%s%s%s",local,text,draftText ? "\n\n尚未应用的项目属性：\n" : "",draftText ? draftText : "");
                else snprintf(combined,need,"当前工程模型：\n%s%s%s",text,draftText ? "\n\n尚未应用的项目属性：\n" : "",draftText ? draftText : "");
            }
        }
        xuiCodeEditSetText(v->controls[XIDE_CHANGES_LOCAL],combined ? combined : local); free(combined); xrtFree(draftText); xrtValueRelease(draft); xrtFree(text); xrtValueRelease(root);
        xideConfigureEditorLanguage(v->controls[XIDE_CHANGES_LOCAL],language); xideConfigureEditorLanguage(v->controls[XIDE_CHANGES_DISK],language);
        xide_text_file f={0}; v->valid=c->snapshot.state==XIDE_WATCH_READY && xideTextFileDecode(c->snapshot.bytes,c->snapshot.length,&f,&e);
        xuiCodeEditSetText(v->controls[XIDE_CHANGES_DISK],v->valid ? f.text : e.message[0] ? e.message : c->reason); xideTextFileClear(&f);
        v->sequence=c->snapshot.sequence; v->localSignature=localSignature(app,c); message(v,c->reason);
    } else message(v,"当前没有待处理磁盘变化。后台继续监视已命名文档、清单和工程引用。");
    enabled(app); xideFileChangesViewLayout(app);
}
static void clearLabels(xide_file_changes_view* v) { for(size_t i=0;i<v->count;++i) free(v->labels[i]); free(v->labels); free(v->tokens); v->labels=NULL; v->tokens=NULL; v->count=0; }
static void rebuild(xide_app* app)
{
    xide_file_changes_view* v=app->fileChangesView; size_t count=xideFileSyncCount(app); char** labels=count ? calloc(count,sizeof(*labels)) : NULL; uint64_t* tokens=count ? calloc(count,sizeof(*tokens)) : NULL; int okay=!count || (labels && tokens),chosen=-1;
    for(size_t i=0;okay && i<count;++i) { const xide_file_change* c=xideFileSyncAt(app,i); size_t n=strlen(c->snapshot.path)+64; labels[i]=malloc(n); okay=labels[i]!=NULL; if(okay) { snprintf(labels[i],n,"%s · %s",xidePathFilename(c->snapshot.path),c->documentId ? "编辑文档" : "工程清单"); tokens[i]=c->snapshot.token; if(tokens[i]==v->token) chosen=(int)i; } }
    if(!okay) { for(size_t i=0;i<count;++i) free(labels ? labels[i] : NULL); free(labels); free(tokens); message(v,"无法更新变化列表；原预览保留，请刷新重试。"); return; }
    v->updating=1;
    if(xuiListViewSetItems(v->controls[XIDE_CHANGES_LIST],(const char**)labels,(int)count)!=XUI_OK) { v->updating=0; for(size_t i=0;i<count;++i) free(labels[i]); free(labels); free(tokens); message(v,"变化列表更新失败；原预览保留。"); return; }
    clearLabels(v); v->labels=labels; v->tokens=tokens; v->count=count; xuiListViewSetSelected(v->controls[XIDE_CHANGES_LIST],chosen); v->updating=0;
    const xide_file_change* c=selected(app); int changed=!c || c->snapshot.sequence!=v->sequence;
    if(changed) { int confirmed=v->confirm; if(!c) v->token=0; preview(app); if(confirmed) message(v,"磁盘版本已变化，原确认已取消；请重新选择、比较后确认。"); }
    v->revision=xideFileSyncRevision(app); enabled(app);
}
static void selectItem(xui_widget widget,int index,void* user)
{ (void)widget; xide_app* app=user; xide_file_changes_view* v=app->fileChangesView; if(v->updating) return; v->token=index>=0 && (size_t)index<v->count ? v->tokens[index] : 0; preview(app); xgeRenderRequest(); }
static void clicked(xui_widget widget,void* user)
{
    xide_app* app=user; xide_file_changes_view* v=app->fileChangesView; const xide_file_change* c=selected(app);
    if(widget==v->controls[XIDE_CHANGES_CLOSE]) { v->confirm=0; xideDialogSetOpen(v->window,0); }
    else if(widget==v->controls[XIDE_CHANGES_CANCEL]) { v->confirm=0; xideDialogSetScroll(v->window,0,0); }
    else if(widget==v->controls[XIDE_CHANGES_CONFIRM] && v->confirm) {
        xide_model_error e={0};
        if(!c || c->snapshot.sequence!=v->sequence || !v->localSignature || localSignature(app,c)!=v->localSignature) { v->confirm=0; preview(app); message(v,"当前内容或磁盘版本已变化，请重新比较后确认。"); }
        else if(xideFileSyncReload(app,v->token,v->sequence,v->documentVersion,1,&e)) { v->confirm=0; v->token=0; rebuild(app); message(v,"已接受所选磁盘快照；文档重载可以撤销。源文件未被写入。"); xideDialogSetScroll(v->window,0,0); }
        else { v->confirm=0; message(v,e.message); }
    } else if(v->confirm) return;
    else if(widget==v->controls[XIDE_CHANGES_RELOAD] && v->valid && c) { v->confirm=1; }
    else if(widget==v->controls[XIDE_CHANGES_KEEP] && c) { if(xideFileSyncKeep(app,v->token,v->sequence)) { v->token=0; rebuild(app); message(v,"已保留当前内容。保存仍核对原始磁盘基线；后续磁盘变化会再次通知。"); } }
    else if(widget==v->controls[XIDE_CHANGES_REFRESH]) { xideFileSyncRefresh(app); message(v,"正在后台重新核对磁盘文件…"); }
    enabled(app); xideFileChangesViewLayout(app); xgeRenderRequest();
}
static int label(xide_app* app,const char* text,xui_rect_t rect,xui_widget* out)
{ xui_label_desc_t d={0}; d.iSize=sizeof(d); d.pFont=app->uiFont; d.sText=text; d.iWrapMode=XUI_TEXT_WRAP_WORD; xui_widget w=NULL; TRY(xuiLabelCreate(app->context,&w,&d)); TRY(xuiWidgetSetRect(w,rect)); TRY(xuiWidgetAddChild(app->fileChangesView->content,w)); if(out) *out=w; return XUI_OK; }
static int button(xide_app* app,int id,const char* text,xui_rect_t rect)
{ xui_button_desc_t d={0}; d.iSize=sizeof(d); d.pFont=app->uiFont; d.sText=text; xui_widget* w=&app->fileChangesView->controls[id]; TRY(xuiButtonCreate(app->context,w,&d)); TRY(xuiWidgetSetRect(*w,rect)); TRY(xuiButtonSetClick(*w,clicked,app)); return xuiWidgetAddChild(app->fileChangesView->content,*w); }
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->fileChangesView->controls[XIDE_CHANGES_CLOSE],a); }
int xideFileChangesViewCreate(xide_app* app)
{
    xide_file_changes_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY; app->fileChangesView=v;
    TRY(xideDialogCreate(app,&v->window,"磁盘变化",760,680,14)); TRY(xideDialogSetCancel(v->window,windowClosed,app)); v->content=xideDialogGetContentWidget(v->window); TRY(xuiWidgetAddChild(app->root,v->window)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
    TRY(label(app,"磁盘变化",(xui_rect_t){0,0,760,28},NULL)); TRY(label(app,"",(xui_rect_t){0,38,760,60},&v->message));
    xui_list_view_desc_t list={0}; list.iSize=sizeof(list); list.pFont=app->uiFont; list.fItemHeight=28; list.iSelected=-1; TRY(xuiListViewCreate(app->context,&v->controls[XIDE_CHANGES_LIST],&list)); TRY(xuiWidgetSetRect(v->controls[XIDE_CHANGES_LIST],(xui_rect_t){0,104,760,112})); TRY(xuiListViewSetSelect(v->controls[XIDE_CHANGES_LIST],selectItem,app)); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_CHANGES_LIST]));
    xui_input_desc_t path={0}; path.iSize=sizeof(path); path.pFont=app->uiFont; TRY(xuiInputCreate(app->context,&v->path,&path)); TRY(xuiInputSetReadonly(v->path,1)); TRY(xuiWidgetSetRect(v->path,(xui_rect_t){0,228,760,30})); TRY(xuiWidgetAddChild(v->content,v->path));
    TRY(label(app,"当前内容（内存）",(xui_rect_t){0,274,370,28},NULL)); TRY(label(app,"磁盘快照（只读）",(xui_rect_t){390,274,370,28},NULL));
    for(int i=XIDE_CHANGES_LOCAL;i<=XIDE_CHANGES_DISK;++i) { xui_code_edit_desc_t code={0}; code.iSize=sizeof(code); code.pFont=app->codeFont; code.bReadonly=1; code.bShowLineNumbers=1; code.sText=""; TRY(xideDpiCodeEditCreate(app->context,&v->controls[i],&code)); TRY(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(v->controls[i]))); TRY(xideConfigureEditorLanguage(v->controls[i],"text")); TRY(xuiWidgetSetRect(v->controls[i],(xui_rect_t){i==XIDE_CHANGES_LOCAL ? 0 : 390,310,370,230})); TRY(xuiWidgetAddChild(v->content,v->controls[i])); }
    TRY(button(app,XIDE_CHANGES_REFRESH,"刷新",(xui_rect_t){0,558,100,32})); TRY(button(app,XIDE_CHANGES_KEEP,"保留当前内容",(xui_rect_t){114,558,170,32})); TRY(button(app,XIDE_CHANGES_RELOAD,"重载磁盘内容…",(xui_rect_t){298,558,200,32})); TRY(button(app,XIDE_CHANGES_CLOSE,"关闭",(xui_rect_t){650,558,110,32}));
    TRY(label(app,"确认重载会放弃所选文件及对应工程清单/属性的未保存修改。删除或改绑的项目引用也会更新；已打开源文档继续保留。",(xui_rect_t){0,612,760,80},&v->warning));
    TRY(button(app,XIDE_CHANGES_CONFIRM,"确认重载",(xui_rect_t){478,704,168,32})); TRY(button(app,XIDE_CHANGES_CANCEL,"取消",(xui_rect_t){660,704,100,32}));
    TRY(xideUiLayoutCapture(&v->layout,v->content)); preview(app); return XUI_OK;
}
int xideFileChangesViewOpen(xide_app* app)
{ if(!app->fileChangesView || xideToolchainViewIsOpen(app) || xideResourceViewIsOpen(app) || xideImportViewIsOpen(app) || xideRenameViewIsOpen(app) || xideTemplateViewIsOpen(app) || xideReferencesViewIsOpen(app) || xideFontViewIsOpen(app) || xideConfigurationViewIsOpen(app) || xideRecoveryViewIsOpen(app)) return 0; xide_file_changes_view* v=app->fileChangesView; v->token=0; v->sequence=0; v->confirm=0; rebuild(app); preview(app); xideFileChangesViewLayout(app); xideDialogSetOpen(v->window,1); xgeRenderRequest(); return 1; }
int xideFileChangesViewIsOpen(xide_app* app) { return app && app->fileChangesView && xideDialogIsOpen(app->fileChangesView->window); }
void xideFileChangesViewPoll(xide_app* app)
{ if(xideFileChangesViewIsOpen(app)) { if(app->fileChangesView->revision!=xideFileSyncRevision(app)) rebuild(app); xideFileChangesViewLayout(app); } }
void xideFileChangesViewLayout(xide_app* app)
{
    xide_file_changes_view* v=app->fileChangesView; if(!v) return; int w=app->viewportWidth-48,h=app->viewportHeight-48; if(w<200) w=200; if(h<200) h=200; xideDialogSetMaxSize(v->window,w,h);
    float scale=xideUiFontScale(app); if(v->scale!=scale) { xideUiLayoutApply(app,&v->layout,1); v->scale=scale; }
    int cw=xideUiHeight(app,760),ch=xideUiHeight(app,v->confirm ? 752 : 604); if(cw!=v->cw || ch!=v->ch) { xideDialogSetContentSize(v->window,cw,ch); v->cw=cw; v->ch=ch; }
    
}
int xideFileChangesViewCodeFont(xide_app* app)
{ if(app->fileChangesView) for(int i=XIDE_CHANGES_LOCAL;i<=XIDE_CHANGES_DISK;++i) TRY(xideCodeWidgetSetFont(app,app->fileChangesView->controls[i],app->codeFont)); return XUI_OK; }
int xideFileChangesViewTheme(xide_app* app)
{
    if(!app->fileChangesView) return XUI_OK;
    const int ids[]={XUI_CODE_STYLE_DEFAULT,XUI_CODE_STYLE_KEYWORD,XUI_CODE_STYLE_TYPE,XUI_CODE_STYLE_NUMBER,XUI_CODE_STYLE_STRING,XUI_CODE_STYLE_CHAR,XUI_CODE_STYLE_COMMENT}; const ide_color_role roles[]={IDE_TEXT,IDE_KEYWORD,IDE_TYPE,IDE_NUMBER,IDE_STRING,IDE_STRING,IDE_COMMENT};
    for(int pane=XIDE_CHANGES_LOCAL;pane<=XIDE_CHANGES_DISK;++pane) for(size_t i=0;i<sizeof(ids)/sizeof(*ids);++i) { xui_code_style_t style={sizeof(style),ideThemeColor(app->theme,roles[i]),ideThemeColor(app->theme,IDE_FIELD),0}; TRY(xuiCodeEditSetStyle(app->fileChangesView->controls[pane],ids[i],&style)); }
    return XUI_OK;
}
void xideFileChangesViewDestroy(xide_app* app) { if(app->fileChangesView) { clearLabels(app->fileChangesView); free(app->fileChangesView); app->fileChangesView=NULL; } }
xui_widget xideFileChangesViewWindow(xide_app* app) { return app->fileChangesView ? app->fileChangesView->window : NULL; }
xui_widget xideFileChangesViewControl(xide_app* app,int id) { return app->fileChangesView && id>=0 && id<XIDE_CHANGES_CONTROL_COUNT ? app->fileChangesView->controls[id] : NULL; }
const char* xideFileChangesViewMessage(xide_app* app) { return app->fileChangesView ? app->fileChangesView->messageText : ""; }
