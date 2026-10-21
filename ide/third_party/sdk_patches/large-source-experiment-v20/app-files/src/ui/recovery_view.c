#include "ui/toolchain_view.h"
#include "platform/dpi_proxy.h"
#include "ui/resource_view.h"
#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "ui/references_view.h"
#include "ui/file_changes_view.h"
#include "ui/recovery_view.h"
#include "services/recovery.h"
#include "services/session.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "ui/font_view.h"
#include "ui/configuration_view.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include "lang/languages.h"
#include "theme/theme.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) return result_; } while(0)
struct xide_recovery_view {
    xui_widget popup,content,title,scope,path,message,confirmText,globalPath,controls[XIDE_RECOVERY_CONTROL_COUNT];
    xide_ui_layout layout; float scale; int confirm,updating,closing,previewValid,contentWidth,contentHeight,repairKind;
    xide_recovery_service* service; xide_recovery_list list; xide_text_file preview; char* previewKey; char* backupPath; char messageText[1024];
};
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static const char* selected(xide_recovery_view* v)
{ int index=xuiListViewGetSelected(v->controls[XIDE_RECOVERY_LIST]); return index>=0 && (size_t)index<v->list.count ? v->list.items[index].reference : NULL; }
static const char* language(xide_recovery_view* v)
{ int i=xuiComboBoxGetSelected(v->controls[XIDE_RECOVERY_LANGUAGE]); return i==0 ? "c" : i==1 ? "xlang" : "text"; }
static const char* globalName(int kind) { return kind==XIDE_GLOBAL_INDEX ? "最近工程索引" : kind==XIDE_GLOBAL_WINDOW ? "主窗口位置" : "程序偏好"; }
static void globalTarget(xide_app* app)
{
    xide_recovery_view* v=app->recoveryView; int kind=xuiComboBoxGetSelected(v->controls[XIDE_RECOVERY_GLOBAL_KIND]);
    char* path=xideSessionGlobalPath(app,kind); xuiEditSetText(v->globalPath,path ? path : "当前启动参数未启用此文件"); free(path);
}
static void message(xide_recovery_view* v,const char* s)
{ snprintf(v->messageText,sizeof(v->messageText),"%s",s); xuiLabelSetText(v->message,v->messageText); }
static void enabled(xide_app* app)
{
    xide_recovery_view* v=app->recoveryView; int busy=xideRecoveryServiceBusy(v->service),available=v->service!=NULL;
    for(int i=0;i<XIDE_RECOVERY_CONTROL_COUNT;++i) xuiWidgetSetEnabled(v->controls[i],available && xideSessionDirectory(app) && !busy && !v->confirm);
    char* target=xideSessionGlobalPath(app,xuiComboBoxGetSelected(v->controls[XIDE_RECOVERY_GLOBAL_KIND]));
    xuiWidgetSetEnabled(v->controls[XIDE_RECOVERY_GLOBAL_KIND],!busy && !v->confirm);
    xuiWidgetSetEnabled(v->controls[XIDE_RECOVERY_GLOBAL_REPAIR],available && target && !busy && !v->confirm); free(target);
    xuiWidgetSetEnabled(v->controls[XIDE_RECOVERY_CLOSE],1);
    xuiWidgetSetEnabled(v->controls[XIDE_RECOVERY_RESTORE],!busy && !v->confirm && v->previewValid && selected(v) && v->previewKey && !strcmp(selected(v),v->previewKey));
    xuiWidgetSetEnabled(v->controls[XIDE_RECOVERY_CODE],!busy);
    xuiWidgetSetEnabled(v->controls[XIDE_RECOVERY_CONFIRM],v->confirm && !busy);
    xuiWidgetSetEnabled(v->controls[XIDE_RECOVERY_CANCEL_REPAIR],v->confirm && !busy);
    xuiWidgetSetVisible(v->controls[XIDE_RECOVERY_CONFIRM],v->confirm);
    xuiWidgetSetVisible(v->controls[XIDE_RECOVERY_CANCEL_REPAIR],v->confirm);
    xuiWidgetSetVisible(v->confirmText,v->confirm);
    /* The pinned SDK can retain a disabled cache after a click handler disables
       its own button. Re-enable input and synchronize the public visual state. */
    const int buttons[]={XIDE_RECOVERY_REFRESH,XIDE_RECOVERY_CLEAN,XIDE_RECOVERY_REPAIR,XIDE_RECOVERY_GLOBAL_REPAIR,XIDE_RECOVERY_RESTORE,XIDE_RECOVERY_CLOSE,XIDE_RECOVERY_CONFIRM,XIDE_RECOVERY_CANCEL_REPAIR};
    const uint32_t states[]={XUI_WIDGET_STATE_DISABLED,XUI_WIDGET_STATE_ACTIVE,XUI_WIDGET_STATE_HOVER,XUI_WIDGET_STATE_FOCUS};
    for(size_t i=0;i<sizeof(buttons)/sizeof(*buttons);++i) {
        xui_widget widget=v->controls[buttons[i]]; uint32_t input=xuiWidgetGetInputState(widget),visual=0;
        for(size_t j=0;j<sizeof(states)/sizeof(*states);++j) if(input&states[j]) { visual=states[j]; break; }
        xuiWidgetSetStateId(widget,visual);
    }
}
static int begin(xide_app* app,int job,const char* ref)
{
    xide_recovery_view* v=app->recoveryView; xide_model_error e={0};
    char* global=NULL;
    if(job==XIDE_RECOVERY_MAINTAIN && !xideSessionMaintenanceRoot(app,&global,&e)) { message(v,e.message); return 0; }
    int started=xideRecoveryServiceStart(v->service,job,global ? global : ref,&e); free(global);
    if(!started) { message(v,e.message); return 0; }
    if(job==XIDE_RECOVERY_REBUILD || job==XIDE_RECOVERY_GLOBAL) { free(v->backupPath); v->backupPath=NULL; }
    if(job==XIDE_RECOVERY_PREVIEW) message(v,"正在读取所选正文…");
    else if(job==XIDE_RECOVERY_SCAN) message(v,"正在扫描当前作用域和备份正文…");
    else if(job==XIDE_RECOVERY_MAINTAIN) message(v,"正在按七天保留规则维护会话和退休迁移正文；当前会话与备份受到保护…");
    else if(job==XIDE_RECOVERY_GLOBAL) message(v,"正在后台备份所选全局文件；提交前可关闭窗口取消…");
    else message(v,"正在后台备份原元数据和全部普通恢复文件；可关闭窗口取消…");
    enabled(app); xgeRenderRequest(); return 1;
}
static void selectItem(xui_widget widget,int index,void* user)
{
    (void)widget; (void)index; xide_app* app=user; xide_recovery_view* v=app->recoveryView;
    if(v->updating || xideRecoveryServiceBusy(v->service)) return;
    v->previewValid=0; xideTextFileClear(&v->preview); free(v->previewKey); v->previewKey=NULL;
    xuiCodeEditSetText(v->controls[XIDE_RECOVERY_CODE],""); const char* ref=selected(v); xuiEditSetText(v->path,ref ? ref : "");
    if(ref) begin(app,XIDE_RECOVERY_PREVIEW,ref); else enabled(app);
}
static void chooseLanguage(xui_widget widget,int index,int value,void* user)
{ (void)widget; (void)index; (void)value; xide_app* app=user; xideConfigureEditorLanguage(app->recoveryView->controls[XIDE_RECOVERY_CODE],language(app->recoveryView)); xgeRenderRequest(); }
static void chooseGlobal(xui_widget widget,int index,int value,void* user)
{ (void)widget; (void)index; (void)value; xide_app* app=user; globalTarget(app); enabled(app); xgeRenderRequest(); }
static void clicked(xui_widget widget,void* user)
{
    xide_app* app=user; xide_recovery_view* v=app->recoveryView;
    if(widget==v->controls[XIDE_RECOVERY_CLOSE]) {
        if(xideRecoveryServiceBusy(v->service)) { v->closing=1; xideRecoveryServiceCancel(v->service); message(v,"正在取消并回退尚未提交的恢复任务…"); }
        else { v->confirm=0; xuiPopupSetOpen(v->popup,0); } xgeRenderRequest(); return;
    }
    if(xideRecoveryServiceBusy(v->service)) return;
    if(widget==v->controls[XIDE_RECOVERY_CANCEL_REPAIR]) v->confirm=0;
    else if(widget==v->controls[XIDE_RECOVERY_CONFIRM] && v->confirm) {
        int global=v->confirm==2; v->confirm=0;
        if(global) { char* path=xideSessionGlobalPath(app,v->repairKind); if(path) begin(app,XIDE_RECOVERY_GLOBAL,path); else message(v,"所选文件已不可用；原资料保留。"); free(path); }
        else begin(app,XIDE_RECOVERY_REBUILD,NULL);
    }
    else if(v->confirm) return;
    else if(widget==v->controls[XIDE_RECOVERY_REPAIR]) { v->confirm=1; xuiLabelSetText(v->confirmText,"确认后完整备份原元数据和普通恢复文件，再按当前打开文档、布局和阅读状态重建会话。旧资料保存在备份中；源文件不被覆盖。"); }
    else if(widget==v->controls[XIDE_RECOVERY_GLOBAL_REPAIR]) {
        v->confirm=2; v->repairKind=xuiComboBoxGetSelected(v->controls[XIDE_RECOVERY_GLOBAL_KIND]);
        char info[1024]; snprintf(info,sizeof(info),"确认后备份并重建%s。%s原始字节保存在独立 state-backups 目录中；当前编辑文档继续保留。",globalName(v->repairKind),v->repairKind==XIDE_GLOBAL_INDEX ? "索引将指向当前工作空间或项目。" : v->repairKind==XIDE_GLOBAL_WINDOW ? "保存当前窗口位置与尺寸。" : "保存当前字体与已加载的工具偏好；启动参数中的临时工具覆盖不写入偏好。"); xuiLabelSetText(v->confirmText,info);
    }
    else if(widget==v->controls[XIDE_RECOVERY_REFRESH]) begin(app,XIDE_RECOVERY_SCAN,NULL);
    else if(widget==v->controls[XIDE_RECOVERY_CLEAN]) begin(app,XIDE_RECOVERY_MAINTAIN,NULL);
    else if(widget==v->controls[XIDE_RECOVERY_RESTORE] && v->previewValid && v->preview.text) {
        xide_model_error e={0}; xide_document* d=xideDocumentNew(&app->documents,language(v),&e);
        int okay=d && xuiCodeDocumentInsert(xuiCodeEditGetDocument(d->editor),0,v->preview.text)==XUI_OK;
        if(okay) { d->languageOverride=1; xuiPopupSetOpen(v->popup,0); okay=xideEditorShow(app,d)==XUI_OK; }
        if(!okay) { if(d) xideDocumentClose(d,1,&e); xuiPopupSetOpen(v->popup,1); message(v,e.message[0] ? e.message : "无法创建取回的未命名文档。"); }
    }
    enabled(app); xideRecoveryViewLayout(app);
    if(widget==v->controls[XIDE_RECOVERY_CANCEL_REPAIR]) xuiPopupSetScroll(v->popup,0,0);
    xgeRenderRequest();
}
static int label(xide_app* app,const char* text,xui_rect_t rect,xui_widget* out)
{
    xui_label_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; desc.sText=text; desc.iWrapMode=XUI_TEXT_WRAP_WORD;
    xui_widget widget=NULL; TRY(xuiLabelCreate(app->context,&widget,&desc)); TRY(xuiWidgetSetRect(widget,rect)); TRY(xuiWidgetAddChild(app->recoveryView->content,widget)); if(out) *out=widget; return XUI_OK;
}
static int button(xide_app* app,int index,const char* text,xui_rect_t rect)
{
    xui_button_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont; desc.sText=text; xui_widget* w=&app->recoveryView->controls[index];
    TRY(xuiButtonCreate(app->context,w,&desc)); TRY(xuiWidgetSetRect(*w,rect)); TRY(xuiButtonSetClick(*w,clicked,app)); return xuiWidgetAddChild(app->recoveryView->content,*w);
}
static int readonlyPath(xide_app* app,const char* name,xui_rect_t rect,xui_widget* out)
{
    xui_input_desc_t desc={0}; desc.iSize=sizeof(desc); desc.pFont=app->uiFont;
    TRY(xuiInputCreate(app->context,out,&desc)); TRY(xuiInputSetReadonly(*out,1)); TRY(xuiWidgetSetAccessibleName(*out,name)); TRY(xuiWidgetSetRect(*out,rect)); return xuiWidgetAddChild(app->recoveryView->content,*out);
}
int xideRecoveryViewCreate(xide_app* app)
{
    xide_recovery_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY; app->recoveryView=v;
    xui_popup_desc_t popup={0}; popup.iSize=sizeof(popup); popup.fContentWidth=740; popup.fContentHeight=580; popup.fPadding=14; popup.fMargin=18;
    popup.iAnchor=XUI_POPUP_ANCHOR_FIXED; popup.bModal=1; popup.bConsumeInside=1; popup.iOutsidePolicy=XUI_POPUP_OUTSIDE_CONSUME; popup.iOwnerPolicy=XUI_POPUP_OWNER_CONSUME; popup.iFocusPolicy=XUI_POPUP_FOCUS_FIRST_CHILD;
    TRY(xuiPopupCreate(app->context,&v->popup,&popup)); v->content=xuiPopupGetContentWidget(v->popup);
    TRY(xuiWidgetAddChild(app->root,v->popup)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
    TRY(label(app,"会话与恢复",(xui_rect_t){0,0,740,28},&v->title));
    TRY(readonlyPath(app,"当前会话作用域",(xui_rect_t){0,34,740,28},&v->scope)); TRY(label(app,"",(xui_rect_t){0,72,740,56},&v->message));
    TRY(button(app,XIDE_RECOVERY_REFRESH,"刷新正文列表",(xui_rect_t){0,134,148,32})); TRY(button(app,XIDE_RECOVERY_CLEAN,"清理过期快照",(xui_rect_t){160,134,164,32})); TRY(button(app,XIDE_RECOVERY_REPAIR,"备份并重建会话…",(xui_rect_t){338,134,224,32}));
    xui_list_view_desc_t list={0}; list.iSize=sizeof(list); list.pFont=app->uiFont; list.fItemHeight=28; list.iSelected=-1;
    TRY(xuiListViewCreate(app->context,&v->controls[XIDE_RECOVERY_LIST],&list)); TRY(xuiWidgetSetRect(v->controls[XIDE_RECOVERY_LIST],(xui_rect_t){0,180,740,112})); TRY(xuiListViewSetSelect(v->controls[XIDE_RECOVERY_LIST],selectItem,app)); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_RECOVERY_LIST]));
    TRY(readonlyPath(app,"所选恢复正文",(xui_rect_t){0,304,740,28},&v->path));
    xui_code_edit_desc_t code={0}; code.iSize=sizeof(code); code.pFont=app->codeFont; code.bReadonly=1; code.bShowLineNumbers=1; code.sText="";
    TRY(xideDpiCodeEditCreate(app->context,&v->controls[XIDE_RECOVERY_CODE],&code)); TRY(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(v->controls[XIDE_RECOVERY_CODE]))); TRY(xideConfigureEditorLanguage(v->controls[XIDE_RECOVERY_CODE],"xlang"));
    TRY(xuiWidgetSetRect(v->controls[XIDE_RECOVERY_CODE],(xui_rect_t){0,344,740,172})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_RECOVERY_CODE]));
    TRY(label(app,"取回语法",(xui_rect_t){0,534,88,28},NULL)); const char* choices[]={"C","xlang","纯文本"}; xui_combobox_desc_t combo={0}; combo.iSize=sizeof(combo); combo.pFont=app->uiFont; combo.arrItems=choices; combo.iItemCount=3; combo.iSelected=1;
    TRY(xuiComboBoxCreate(app->context,&v->controls[XIDE_RECOVERY_LANGUAGE],&combo)); TRY(xuiComboBoxSetSelect(v->controls[XIDE_RECOVERY_LANGUAGE],chooseLanguage,app)); TRY(xuiWidgetSetRect(v->controls[XIDE_RECOVERY_LANGUAGE],(xui_rect_t){96,532,170,32})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_RECOVERY_LANGUAGE]));
    TRY(button(app,XIDE_RECOVERY_RESTORE,"取回为未命名文件",(xui_rect_t){420,532,202,32})); TRY(button(app,XIDE_RECOVERY_CLOSE,"关闭",(xui_rect_t){636,532,104,32}));
    TRY(label(app,"全局文件",(xui_rect_t){0,584,88,28},NULL)); const char* globals[]={"最近工程索引","主窗口位置","程序偏好"}; combo.arrItems=globals; combo.iSelected=0;
    TRY(xuiComboBoxCreate(app->context,&v->controls[XIDE_RECOVERY_GLOBAL_KIND],&combo)); TRY(xuiComboBoxSetSelect(v->controls[XIDE_RECOVERY_GLOBAL_KIND],chooseGlobal,app)); TRY(xuiWidgetSetRect(v->controls[XIDE_RECOVERY_GLOBAL_KIND],(xui_rect_t){96,580,248,32})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_RECOVERY_GLOBAL_KIND]));
    TRY(button(app,XIDE_RECOVERY_GLOBAL_REPAIR,"备份并重建所选文件…",(xui_rect_t){396,580,344,32}));
    TRY(readonlyPath(app,"所选全局文件实际路径",(xui_rect_t){0,628,740,28},&v->globalPath));
    TRY(label(app,"",(xui_rect_t){0,680,740,116},&v->confirmText));
    TRY(button(app,XIDE_RECOVERY_CONFIRM,"确认备份并重建",(xui_rect_t){394,810,226,32})); TRY(button(app,XIDE_RECOVERY_CANCEL_REPAIR,"取消修复",(xui_rect_t){634,810,106,32}));
    TRY(xideUiLayoutCapture(&v->layout,v->content)); enabled(app); return XUI_OK;
}
int xideRecoveryViewOpen(xide_app* app)
{
    xide_recovery_view* v=app->recoveryView; if(!v || xideToolchainViewIsOpen(app) || xideResourceViewIsOpen(app) || xideImportViewIsOpen(app) || xideRenameViewIsOpen(app) || xideTemplateViewIsOpen(app) || xideReferencesViewIsOpen(app) || xideFileChangesViewIsOpen(app) || xideFontViewIsOpen(app) || xideConfigurationViewIsOpen(app) || xideRecoveryViewBusy(app)) return 0;
    xideRecoveryServiceDestroy(v->service); v->service=NULL;
    v->updating=1; xuiListViewSetItems(v->controls[XIDE_RECOVERY_LIST],NULL,0); v->updating=0;
    xideRecoveryListClear(&v->list); xideTextFileClear(&v->preview); free(v->previewKey); v->previewKey=NULL; v->previewValid=0;
    free(v->backupPath); v->backupPath=NULL; xuiCodeEditSetText(v->controls[XIDE_RECOVERY_CODE],""); xuiEditSetText(v->path,"");
    const char* directory=xideSessionDirectory(app);
    v->service=xideRecoveryServiceCreate(directory); if(!v->service) return 0;
    v->closing=0; v->confirm=0; globalTarget(app); xuiEditSetText(v->scope,directory ? directory : "会话持久化未启用；可单独修复程序偏好"); xideRecoveryViewLayout(app); xuiPopupSetOpen(v->popup,1);
    if(!directory) { message(v,"当前会话持久化未启用；在全局文件中选择程序偏好可单独备份与修复。"); enabled(app); xgeRenderRequest(); return 1; }
    const char* issue=xideSessionIssue(app); if(issue && *issue) message(v,issue); return begin(app,XIDE_RECOVERY_SCAN,NULL);
}
int xideRecoveryViewIsOpen(xide_app* app) { return app && app->recoveryView && xuiPopupIsOpen(app->recoveryView->popup); }
int xideRecoveryViewBusy(xide_app* app) { return app && app->recoveryView && xideRecoveryServiceBusy(app->recoveryView->service); }
void xideRecoveryViewPoll(xide_app* app)
{
    xide_recovery_view* v=app->recoveryView; if(!v || !v->service) return;
    xide_recovery_result r={0};
    if(!xideRecoveryServiceTake(v->service,&r)) { if(xideRecoveryViewBusy(app)) xgeRenderRequest(); return; }
    if(v->closing) { xideRecoveryResultClear(&r,1); v->closing=0; xuiPopupSetOpen(v->popup,0); enabled(app); return; }
    if(r.job==XIDE_RECOVERY_REBUILD && r.backup.directory) { free(v->backupPath); v->backupPath=copy(r.backup.directory); }
    if(r.job==XIDE_RECOVERY_GLOBAL && r.global.directory) { free(v->backupPath); v->backupPath=copy(r.global.directory); }
    if(!r.okay) {
        if((r.job==XIDE_RECOVERY_REBUILD || r.job==XIDE_RECOVERY_GLOBAL) && v->backupPath) { char info[1024]; snprintf(info,sizeof(info),"%.400s；备份目录：%.580s",r.error.message,v->backupPath); message(v,info); }
        else message(v,r.error.message);
    }
    else if(r.job==XIDE_RECOVERY_SCAN) {
        const char** labels=r.list.count ? calloc(r.list.count,sizeof(*labels)) : NULL;
        if(r.list.count && !labels) message(v,"无法显示正文列表；现有列表保留。");
        else {
            for(size_t i=0;i<r.list.count;++i) labels[i]=r.list.items[i].label;
            v->updating=1; xuiListViewSetItems(v->controls[XIDE_RECOVERY_LIST],labels,(int)r.list.count); xideRecoveryListClear(&v->list); v->list=r.list; memset(&r.list,0,sizeof(r.list));
            v->previewValid=0; xideTextFileClear(&v->preview); free(v->previewKey); v->previewKey=NULL; xuiCodeEditSetText(v->controls[XIDE_RECOVERY_CODE],"");
            xuiListViewSetSelected(v->controls[XIDE_RECOVERY_LIST],v->list.count ? 0 : -1); v->updating=0;
            char summary[768]; snprintf(summary,sizeof(summary),"找到 %zu 份正文。%s",v->list.count,r.error.message[0] ? r.error.message : xideSessionIssue(app)); message(v,summary);
            /* Selection is explicit: scanning preserves the protection reason until a preview is chosen. */
            xuiListViewSetSelected(v->controls[XIDE_RECOVERY_LIST],-1);
        }
        free(labels);
    } else if(r.job==XIDE_RECOVERY_PREVIEW) {
        const char* ref=selected(v);
        if(ref && r.reference && !strcmp(ref,r.reference)) {
            xideTextFileClear(&v->preview); v->preview=r.preview; memset(&r.preview,0,sizeof(r.preview)); free(v->previewKey); v->previewKey=copy(ref);
            v->previewValid=v->previewKey && xuiCodeEditSetText(v->controls[XIDE_RECOVERY_CODE],v->preview.text)==XUI_OK;
            char info[512]; snprintf(info,sizeof(info),"所选正文只读。取回将创建未命名的 %s 文档。",language(v)); message(v,info);
        }
    } else if(r.job==XIDE_RECOVERY_MAINTAIN) {
        char info[320]; snprintf(info,sizeof(info),"已删除 %zu 份过期孤立快照、%zu 份退休迁移正文，释放 %llu 字节；保留 %zu，跳过 %zu。",r.stats.removed,r.migrationStats.removed,(unsigned long long)(r.stats.removedBytes+r.migrationStats.removedBytes),r.stats.retained+r.migrationStats.retained,r.stats.skipped+r.migrationStats.skipped); message(v,info);
    } else if(r.job==XIDE_RECOVERY_GLOBAL) {
        xide_model_error e={0}; int published=xideSessionGlobalPublish(app,v->repairKind,&r.global,&e); char info[1024];
        if(published) snprintf(info,sizeof(info),"%s已重建。原资料备份：%.800s",globalName(v->repairKind),v->backupPath ? v->backupPath : "");
        else snprintf(info,sizeof(info),"全局修复未完成%s：%.300s；备份：%.580s",r.global.published ? "（文件已保存，重新加载失败）" : "",e.message,v->backupPath ? v->backupPath : "");
        message(v,info); globalTarget(app);
    } else if(r.job==XIDE_RECOVERY_REBUILD) {
        xide_model_error e={0};
        int published=xideSessionRebuild(app,&r.backup.guard,&e);
        char info[1024];
        if(published) snprintf(info,sizeof(info),"当前会话已重建。原资料备份：%s",v->backupPath ? v->backupPath : "");
        else snprintf(info,sizeof(info),"备份已完成，但新会话保存失败：%.300s；备份：%.600s",e.message,v->backupPath ? v->backupPath : "");
        message(v,info);
        /* A failed publish retains a complete archive; never overwrite a newly published file during rollback. */
        r.backup.retired=0;
    }
    int repairFinished=r.job==XIDE_RECOVERY_REBUILD || r.job==XIDE_RECOVERY_GLOBAL;
    xideRecoveryResultClear(&r,0); enabled(app); xideRecoveryViewLayout(app);
    if(repairFinished) xuiPopupSetScroll(v->popup,0,0);
    xgeRenderRequest();
}
void xideRecoveryViewLayout(xide_app* app)
{
    xide_recovery_view* v=app->recoveryView; if(!v) return; int width=app->viewportWidth-48,height=app->viewportHeight-48; if(width<200) width=200; if(height<200) height=200;
    xuiPopupSetMaxSize(v->popup,width,height); float scale=xideUiFontScale(app); if(v->scale!=scale) { xideUiLayoutApply(app,&v->layout,1); v->scale=scale; }
    int cw=xideUiHeight(app,740),ch=xideUiHeight(app,v->confirm ? 860 : 676);
    if(cw!=v->contentWidth || ch!=v->contentHeight) { xuiPopupSetContentSize(v->popup,cw,ch); v->contentWidth=cw; v->contentHeight=ch; }
    int sw=width<cw+28 ? width : cw+28,sh=height<ch+28 ? height : ch+28;
    xuiPopupSetAnchorRect(v->popup,(xui_rect_t){(app->viewportWidth-sw)/2,(app->viewportHeight-sh)/2,0,0});
}
int xideRecoveryViewCodeFont(xide_app* app) { return app->recoveryView ? xideCodeWidgetSetFont(app,app->recoveryView->controls[XIDE_RECOVERY_CODE],app->codeFont) : XUI_OK; }
int xideRecoveryViewTheme(xide_app* app)
{
    if(!app->recoveryView) return XUI_OK;
    const int ids[]={XUI_CODE_STYLE_DEFAULT,XUI_CODE_STYLE_KEYWORD,XUI_CODE_STYLE_TYPE,XUI_CODE_STYLE_NUMBER,XUI_CODE_STYLE_STRING,XUI_CODE_STYLE_CHAR,XUI_CODE_STYLE_COMMENT};
    const ide_color_role roles[]={IDE_TEXT,IDE_KEYWORD,IDE_TYPE,IDE_NUMBER,IDE_STRING,IDE_STRING,IDE_COMMENT};
    for(size_t i=0;i<sizeof(ids)/sizeof(*ids);++i) { xui_code_style_t style={sizeof(style),ideThemeColor(app->theme,roles[i]),ideThemeColor(app->theme,IDE_FIELD),0}; TRY(xuiCodeEditSetStyle(app->recoveryView->controls[XIDE_RECOVERY_CODE],ids[i],&style)); } return XUI_OK;
}
void xideRecoveryViewStop(xide_app* app) { if(app->recoveryView) { xideRecoveryServiceDestroy(app->recoveryView->service); app->recoveryView->service=NULL; } }
void xideRecoveryViewDestroy(xide_app* app)
{ xide_recovery_view* v=app->recoveryView; if(!v) return; xideRecoveryServiceDestroy(v->service); xideRecoveryListClear(&v->list); xideTextFileClear(&v->preview); free(v->previewKey); free(v->backupPath); free(v); app->recoveryView=NULL; }
xui_widget xideRecoveryViewPopup(xide_app* app) { return app->recoveryView ? app->recoveryView->popup : NULL; }
xui_widget xideRecoveryViewControl(xide_app* app,int control) { return app->recoveryView && control>=0 && control<XIDE_RECOVERY_CONTROL_COUNT ? app->recoveryView->controls[control] : NULL; }
const char* xideRecoveryViewMessage(xide_app* app) { return app->recoveryView ? app->recoveryView->messageText : ""; }
const char* xideRecoveryViewBackupPath(xide_app* app) { return app->recoveryView ? app->recoveryView->backupPath : NULL; }
