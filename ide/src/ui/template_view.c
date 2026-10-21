#define _WIN32_WINNT 0x0601
#include "ui/toolchain_view.h"
#include "platform/dpi_proxy.h"
#include "ui/resource_view.h"
#include "ui/import_view.h"
#include "ui/template_view.h"
#include "ui/rename_view.h"
#include "services/project_templates.h"
#include "services/ui_font.h"
#include "services/editor_font.h"
#include "ui/workspace_view.h"
#include "ui/reading_state.h"
#include "ui/editor_view.h"
#include "ui/references_view.h"
#include "ui/configuration_view.h"
#include "ui/font_view.h"
#include "ui/recovery_view.h"
#include "ui/file_changes_view.h"
#include "lang/languages.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TRY(call) do { int r_=(call); if(r_!=XUI_OK) return r_; } while(0)
struct xide_template_view {
 xui_widget window,content,controls[XIDE_TEMPLATE_CONTROL_COUNT],destination,description,message;
 xide_ui_layout layout; float scale; xide_template_plan* plan; xvalue* baseline;
 const char *templateTitles[4],*fileTitles[8]; int sync,initialized; char messageText[512];
};
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s ? s : ""))); }
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static void message(xide_template_view* v,const char* s) { snprintf(v->messageText,sizeof(v->messageText),"%s",s); xuiLabelSetText(v->message,v->messageText); xgeRenderRequest(); }
static int putFile(xvalue* list,const xide_file* f)
{
 xvalue* o=xrtValueObject(); int okay=o && put(o,"id",f->id) && put(o,"path",f->path) && put(o,"kind",f->kind) && xrtValueObjectSet(o,sv("source"),f->source);
 if(okay) { return xrtValueArrayAppendNew(list,o); } xrtValueRelease(o); return 0;
}
/* Includes unnamed workspace state and full project data; no mutable pointer is
   retained across modal callbacks or background file coordination. */
static xvalue* baseline(xide_app* a)
{
 xide_workspace* w=&a->workspaceModel; xvalue *o=xrtValueObject(),*projects=xrtValueArray(),*files=xrtValueArray();
 int okay=o && projects && files && put(o,"name",w->name) && put(o,"manifest",w->manifest) && put(o,"startup",w->startupProject) && xrtValueObjectSetNew(o,sv("dirty"),xrtValueBool(w->dirty!=0)) && xrtValueObjectSetNew(o,sv("loaded"),xrtValueBool(w->source!=NULL)) && (!w->source || xrtValueObjectSet(o,sv("source"),w->source));
 for(size_t i=0;okay && i<w->projectCount;++i) {
  xide_project* p=&w->projects[i]; xvalue* item=xrtValueObject(); xvalue* data=p->available ? xideProjectExport(p,NULL) : p->source ? xrtValueDeepClone(p->source) : xrtValueObject();
  okay=item && data && put(item,"id",p->id) && put(item,"manifest",p->manifest) && put(item,"issue",p->issue) && xrtValueObjectSetNew(item,sv("dirty"),xrtValueBool(p->dirty!=0)) && xrtValueObjectSetNew(item,sv("available"),xrtValueBool(p->available!=0)) && xrtValueObjectSet(item,sv("data"),data) && xrtValueObjectSet(item,sv("reference"),p->referenceSource);
  xrtValueRelease(data); if(okay) okay=xrtValueArrayAppendNew(projects,item); else xrtValueRelease(item);
 }
 for(size_t i=0;okay && i<w->files.count;++i) okay=putFile(files,&w->files.items[i]);
 if(okay) okay=xrtValueObjectSet(o,sv("projects"),projects) && xrtValueObjectSet(o,sv("files"),files);
 xrtValueRelease(projects); xrtValueRelease(files); if(!okay) { xrtValueRelease(o); return NULL; } xvalue* independent=xrtValueDeepClone(o); xrtValueRelease(o); return independent;
}
static void preview(xide_app* a)
{
 xide_template_view* v=a->templateView; int index=xuiComboBoxGetSelected(v->controls[XIDE_TEMPLATE_FILES]); const xide_project_template* d=xideTemplatePlanDefinition(v->plan);
 const char *body="",*kind="text";
 if(d && index>=0 && (size_t)index<d->fileCount) { body=d->files[index].content; kind=d->files[index].kind; }
 else if(d && (size_t)index==d->fileCount) body=xideTemplatePlanManifest(v->plan);
 xuiCodeEditSetText(v->controls[XIDE_TEMPLATE_PREVIEW],body);
 /* Token and fold caches must describe the replacement document version. */
 xideConfigureEditorLanguage(v->controls[XIDE_TEMPLATE_PREVIEW],!strcmp(kind,"xlang") ? "xlang" : !strcmp(kind,"c") || !strcmp(kind,"header") ? "c" : "text");
 xuiCodeEditSetScroll(v->controls[XIDE_TEMPLATE_PREVIEW],0,0);
}
static void createState(xide_template_view* v)
{
 xui_widget w=v->controls[XIDE_TEMPLATE_CREATE]; xuiWidgetSetEnabled(w,v->plan!=NULL);
 uint32_t input=xuiWidgetGetInputState(w),state=0; const uint32_t states[]={XUI_WIDGET_STATE_DISABLED,XUI_WIDGET_STATE_ACTIVE,XUI_WIDGET_STATE_HOVER,XUI_WIDGET_STATE_FOCUS};
 for(size_t i=0;i<sizeof(states)/sizeof(*states);++i) { if(input&states[i]) { state=states[i]; break; } }
 xuiWidgetSetStateId(w,state);
}
int xideTemplateViewRefresh(xide_app* a)
{
 xide_template_view* v=a->templateView; if(!v || v->sync) { return v!=NULL; }
 int index=xuiComboBoxGetSelected(v->controls[XIDE_TEMPLATE_KIND]),selected=xuiComboBoxGetSelected(v->controls[XIDE_TEMPLATE_FILES]); const xide_project_template* d=index>=0 ? xideProjectTemplateAt((size_t)index) : NULL;
 xide_model_error error={0}; xide_template_plan* plan=d ? xideTemplatePlanPrepare(xuiEditGetText(v->controls[XIDE_TEMPLATE_PARENT]),xuiEditGetText(v->controls[XIDE_TEMPLATE_NAME]),d->id,&error) : NULL;
 xideTemplatePlanDestroy(v->plan); v->plan=plan; xuiLabelSetText(v->description,d ? d->description : "请选择项目模板。"); xuiEditSetText(v->destination,plan ? xideTemplatePlanDirectory(plan) : "");
 size_t count=d ? d->fileCount+1 : 0; for(size_t i=0;d && i<d->fileCount;++i) v->fileTitles[i]=d->files[i].path; if(d) v->fileTitles[d->fileCount]="project.xlp";
 v->sync=1; int okay=xuiComboBoxSetItems(v->controls[XIDE_TEMPLATE_FILES],v->fileTitles,(int)count)==XUI_OK;
 if(selected<0 || (size_t)selected>=count) { selected=0; } xuiComboBoxSetSelected(v->controls[XIDE_TEMPLATE_FILES],count ? selected : -1); v->sync=0;
 if(!okay) { xideTemplatePlanDestroy(v->plan); v->plan=NULL; message(v,"无法更新模板文件列表，请重新选择模板。"); } else message(v,plan ? "创建会在新目录生成所列文件并保存项目清单，工作空间引用仍需另行保存。" : error.message[0] ? error.message : "请填写名称与父目录。");
 preview(a); createState(v); return v->plan!=NULL;
}
static int changed(xui_widget w,const xui_edit_event_t* event,void* user)
{ (void)w; if(event->iType==XUI_EDIT_EVENT_TEXT_CHANGED) { xideTemplateViewRefresh(user); } return XUI_OK; }
static void chosen(xui_widget w,int index,int value,void* user)
{ (void)index; (void)value; xide_app* a=user; if(a->templateView->sync) { return; } if(w==a->templateView->controls[XIDE_TEMPLATE_FILES]) preview(a); else xideTemplateViewRefresh(a); xgeRenderRequest(); }
int xideTemplateViewApply(xide_app* a,xide_model_error* e)
{
 xide_template_view* v=a->templateView; if(!v || !xideTemplateViewIsOpen(a)) { return fail(e,XIDE_MODEL_INVALID,"新建项目向导尚未打开。"); }
 xvalue* now=baseline(a); int same=now && v->baseline && xrtValueEqual(now,v->baseline); xrtValueRelease(now);
 if(!same || xideWorkspaceViewHasDraft(a)) { return fail(e,XIDE_MODEL_CONFLICT,"工作空间或项目属性已改变。当前输入保留，请取消并重新打开向导。"); }
 for(size_t i=0;i<a->documents.count;++i) { xide_document* doc=a->documents.items[i]; if(doc->open && doc->dirty && doc->path && a->workspaceModel.manifest && xidePathEqual(doc->path,a->workspaceModel.manifest)) { return fail(e,XIDE_MODEL_CONFLICT,"工作空间清单存在未保存正文，请先处理正文修改。"); } }
 int index=xuiComboBoxGetSelected(v->controls[XIDE_TEMPLATE_KIND]); const xide_project_template* d=index>=0 ? xideProjectTemplateAt((size_t)index) : NULL; const xide_project* prepared=xideTemplatePlanProject(v->plan);
 char* parent=xidePathResolve(NULL,xuiEditGetText(v->controls[XIDE_TEMPLATE_PARENT])); char* planned=v->plan ? xidePathDirectory(xideTemplatePlanDirectory(v->plan)) : NULL;
 int fields=parent && planned && prepared && d && !strcmp(d->id,xideTemplatePlanDefinition(v->plan)->id) && !strcmp(prepared->name,xuiEditGetText(v->controls[XIDE_TEMPLATE_NAME])) && xidePathEqual(parent,planned); free(parent); free(planned);
 if(!fields && !xideTemplateViewRefresh(a)) { return fail(e,XIDE_MODEL_INVALID,v->messageText); }
 xvalue* reading=xideReadingCapture(a,NULL); if(!reading) { return fail(e,XIDE_MODEL_MEMORY,"无法保存工作空间阅读状态。"); }
 xide_workspace fresh={0}; xide_workspace* target=&a->workspaceModel; if(!target->source) { if(!xideWorkspaceCreate(&fresh,"工作空间",e)) { xrtValueRelease(reading); return 0; } target=&fresh; }
 char* id=NULL; int okay=xideTemplatePlanCommit(target,v->plan,xuiCheckBoxGetChecked(v->controls[XIDE_TEMPLATE_STARTUP]),&id,e);
 if(!okay) { xideWorkspaceClear(&fresh); xrtValueRelease(reading); return 0; }
 if(target==&fresh) { xideWorkspaceClear(&a->workspaceModel); a->workspaceModel=fresh; memset(&fresh,0,sizeof(fresh)); }
 xideDialogSetOpen(v->window,0); int refreshed=xideWorkspaceViewRefresh(a); if(refreshed!=XUI_OK) a->frameError=refreshed;
 if(refreshed==XUI_OK) { xideReadingRestore(a,reading,0); xideWorkbenchLayout(a); if(xuiLayout(a->context)==XUI_OK) xideReadingRestore(a,reading,1); xideWorkspaceViewSelectProject(a,id); }
 xrtValueRelease(reading); xide_project* p=xideWorkspaceFindProject(&a->workspaceModel,id);
 if(p && !xideEditorOpen(a,p->entry,e)) { snprintf(a->editorStatus,sizeof(a->editorStatus),"项目已生成；入口打开失败，可从工作空间树重试。"); xuiStatusBarSetItemText(a->statusBar,0,a->editorStatus); }
 free(id); xideTemplatePlanDestroy(v->plan); v->plan=NULL; xideActionsRefresh(a); xgeRenderRequest(); return 1;
}
static void parentSelected(xide_app* a,const char* path,void* user)
{ if(path && a->templateView) { xuiEditSetText(a->templateView->controls[XIDE_TEMPLATE_PARENT],path);xideTemplateViewRefresh(a); } }
static void browse(xide_app* a)
{ xideFileDialogChoose(a,a->templateView->window,XUI_FILE_DIALOG_MODE_SELECT_FOLDER,"选择新项目的父目录",NULL,xuiEditGetText(a->templateView->controls[XIDE_TEMPLATE_PARENT]),NULL,parentSelected,a); }
static void clicked(xui_widget w,void* user)
{
 xide_app* a=user; xide_template_view* v=a->templateView;
 if(w==v->controls[XIDE_TEMPLATE_CANCEL]) { xideDialogSetOpen(v->window,0); xideTemplatePlanDestroy(v->plan); v->plan=NULL; xideActionsRefresh(a); }
 else if(w==v->controls[XIDE_TEMPLATE_BROWSE]) browse(a);
 else if(w==v->controls[XIDE_TEMPLATE_CREATE]) { xide_model_error e={0}; if(!xideTemplateViewApply(a,&e)) message(v,e.message); }
 xgeRenderRequest();
}
static int label(xide_app* a,xui_widget* out,const char* s,xui_rect_t r)
{ xui_widget w=NULL; xui_label_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=s; d.iWrapMode=XUI_TEXT_WRAP_WORD; TRY(xuiLabelCreate(a->context,&w,&d)); TRY(xuiWidgetSetRect(w,r)); TRY(xuiWidgetAddChild(a->templateView->content,w)); if(out) *out=w; return XUI_OK; }
static int input(xide_app* a,xui_widget* out,const char* name,xui_rect_t r,int readonly)
{ xui_input_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; TRY(xuiInputCreate(a->context,out,&d)); TRY(xuiWidgetSetRect(*out,r)); TRY(xuiWidgetSetAccessibleName(*out,name)); TRY(xuiInputSetReadonly(*out,readonly)); if(!readonly) { TRY(xuiEditSetEvent(*out,changed,a)); } return xuiWidgetAddChild(a->templateView->content,*out); }
static int button(xide_app* a,int i,const char* s,xui_rect_t r)
{ xui_button_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=s; TRY(xuiButtonCreate(a->context,&a->templateView->controls[i],&d)); TRY(xuiWidgetSetRect(a->templateView->controls[i],r)); TRY(xuiButtonSetClick(a->templateView->controls[i],clicked,a)); return xuiWidgetAddChild(a->templateView->content,a->templateView->controls[i]); }
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->templateView->controls[XIDE_TEMPLATE_CANCEL],a); }
int xideTemplateViewCreate(xide_app* a)
{
 xide_template_view* v=calloc(1,sizeof(*v)); if(!v) { return XUI_ERROR_OUT_OF_MEMORY; } a->templateView=v;
 TRY(xideDialogCreate(a,&v->window,"新建 xlang 项目",736,672,14)); TRY(xideDialogSetCancel(v->window,windowClosed,a)); v->content=xideDialogGetContentWidget(v->window); TRY(xuiWidgetAddChild(a->root,v->window)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
 TRY(label(a,NULL,"新建 xlang 项目",(xui_rect_t){0,0,736,28})); TRY(label(a,NULL,"项目模板",(xui_rect_t){0,38,120,28}));
 for(size_t i=0;i<4;++i) v->templateTitles[i]=xideProjectTemplateAt(i)->title;
 xui_combobox_desc_t combo={0}; combo.iSize=sizeof(combo); combo.pFont=a->uiFont; combo.arrItems=v->templateTitles; combo.iItemCount=4;
 TRY(xuiComboBoxCreate(a->context,&v->controls[XIDE_TEMPLATE_KIND],&combo)); TRY(xuiWidgetSetRect(v->controls[XIDE_TEMPLATE_KIND],(xui_rect_t){128,34,608,32})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_TEMPLATE_KIND])); TRY(xuiComboBoxSetSelect(v->controls[XIDE_TEMPLATE_KIND],chosen,a));
 TRY(label(a,&v->description,"",(xui_rect_t){0,76,736,48})); TRY(label(a,NULL,"项目名称",(xui_rect_t){0,138,120,28})); TRY(input(a,&v->controls[XIDE_TEMPLATE_NAME],"项目名称与新目录",(xui_rect_t){128,130,608,32},0));
 TRY(label(a,NULL,"创建位置",(xui_rect_t){0,182,120,28})); TRY(input(a,&v->controls[XIDE_TEMPLATE_PARENT],"已有父目录",(xui_rect_t){128,174,500,32},0)); TRY(button(a,XIDE_TEMPLATE_BROWSE,"选择目录…",(xui_rect_t){638,174,98,32}));
 TRY(label(a,NULL,"项目目录",(xui_rect_t){0,224,120,28})); TRY(input(a,&v->destination,"将生成的完整项目目录",(xui_rect_t){128,216,608,32},1));
 xui_checkbox_desc_t check={0}; check.iSize=sizeof(check); check.pFont=a->uiFont; check.sText="设为启动项目"; check.fIndicatorSize=18; check.fGap=8; check.bUseBuiltinAtlas=1;
 TRY(xuiCheckBoxCreate(a->context,&v->controls[XIDE_TEMPLATE_STARTUP],&check)); TRY(xuiWidgetSetRect(v->controls[XIDE_TEMPLATE_STARTUP],(xui_rect_t){0,258,736,32})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_TEMPLATE_STARTUP]));
 TRY(label(a,NULL,"生成文件",(xui_rect_t){0,304,120,28})); combo.arrItems=NULL; combo.iItemCount=0; TRY(xuiComboBoxCreate(a->context,&v->controls[XIDE_TEMPLATE_FILES],&combo)); TRY(xuiWidgetSetRect(v->controls[XIDE_TEMPLATE_FILES],(xui_rect_t){128,296,608,32})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_TEMPLATE_FILES])); TRY(xuiComboBoxSetSelect(v->controls[XIDE_TEMPLATE_FILES],chosen,a));
 xui_code_edit_desc_t code={0}; code.iSize=sizeof(code); code.pFont=a->codeFont; code.bReadonly=1; code.bShowLineNumbers=1; code.sText="";
 TRY(xideDpiCodeEditCreate(a->context,&v->controls[XIDE_TEMPLATE_PREVIEW],&code)); TRY(xideRegisterLanguages(xuiCodeEditGetLanguageRegistry(v->controls[XIDE_TEMPLATE_PREVIEW]))); TRY(xideConfigureEditorLanguage(v->controls[XIDE_TEMPLATE_PREVIEW],"text")); TRY(xuiWidgetSetRect(v->controls[XIDE_TEMPLATE_PREVIEW],(xui_rect_t){0,338,736,230})); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_TEMPLATE_PREVIEW]));
 TRY(label(a,&v->message,"",(xui_rect_t){0,580,736,48})); TRY(button(a,XIDE_TEMPLATE_CREATE,"创建项目",(xui_rect_t){510,636,110,32})); TRY(button(a,XIDE_TEMPLATE_CANCEL,"取消",(xui_rect_t){636,636,100,32}));
 return xideUiLayoutCapture(&v->layout,v->content);
}
int xideTemplateViewOpen(xide_app* a)
{
 if(xideTemplateViewIsOpen(a)) { return 1; } xide_template_view* v=a->templateView;
 if(!v || !a->workspaceView || xideWorkspaceViewHasDraft(a) || xideToolchainViewIsOpen(a) || xideResourceViewIsOpen(a) || xideImportViewIsOpen(a) || xideRenameViewIsOpen(a) || xideReferencesViewIsOpen(a) || xideConfigurationViewIsOpen(a) || xideFontViewIsOpen(a) || xideRecoveryViewIsOpen(a) || xideFileChangesViewIsOpen(a)) { return 0; }
 xvalue* state=baseline(a); if(!state) { return 0; } xrtValueRelease(v->baseline); v->baseline=state;
 if(!v->initialized) { char* parent=a->workspaceModel.manifest ? xidePathDirectory(a->workspaceModel.manifest) : xidePathResolve(NULL,"."); v->sync=1; xuiEditSetText(v->controls[XIDE_TEMPLATE_NAME],"MyProject"); xuiEditSetText(v->controls[XIDE_TEMPLATE_PARENT],parent ? parent : ""); xuiComboBoxSetSelected(v->controls[XIDE_TEMPLATE_KIND],1); v->sync=0; free(parent); v->initialized=1; }
 xuiCheckBoxSetChecked(v->controls[XIDE_TEMPLATE_STARTUP],!a->workspaceModel.startupProject || !*a->workspaceModel.startupProject); xideTemplateViewRefresh(a); xideTemplateViewLayout(a); xideDialogSetScroll(v->window,0,0); xideDialogSetOpen(v->window,1); xuiSetFocusWidget(a->context,v->controls[XIDE_TEMPLATE_NAME]); xideActionsRefresh(a); xgeRenderRequest(); return 1;
}
int xideTemplateViewIsOpen(xide_app* a) { return a && a->templateView && xideDialogIsOpen(a->templateView->window); }
void xideTemplateViewLayout(xide_app* a)
{
 xide_template_view* v=a->templateView; if(!v) { return; } int width=a->viewportWidth-48,height=a->viewportHeight-48; if(width<200) width=200; if(height<200) height=200;
 xideDialogSetMaxSize(v->window,(float)width,(float)height); float scale=xideUiFontScale(a);
 if(v->scale!=scale) { xideUiLayoutApply(a,&v->layout,1); xideDialogSetContentSize(v->window,xideUiHeight(a,736),xideUiHeight(a,672)); v->scale=scale; }
 
}
int xideTemplateViewCodeFont(xide_app* a) { return a->templateView ? xideCodeWidgetSetFont(a,a->templateView->controls[XIDE_TEMPLATE_PREVIEW],a->codeFont) : XUI_OK; }
void xideTemplateViewDestroy(xide_app* a) { xide_template_view* v=a->templateView; if(v) { xideTemplatePlanDestroy(v->plan); xrtValueRelease(v->baseline); free(v); a->templateView=NULL; } }
xui_widget xideTemplateViewWindow(xide_app* a) { return a->templateView ? a->templateView->window : NULL; }
xui_widget xideTemplateViewControl(xide_app* a,int i) { return a->templateView && i>=0 && i<XIDE_TEMPLATE_CONTROL_COUNT ? a->templateView->controls[i] : NULL; }
const char* xideTemplateViewMessage(xide_app* a) { return a->templateView ? a->templateView->messageText : ""; }
