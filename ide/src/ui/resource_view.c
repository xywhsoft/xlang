#include "ui/resource_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "services/ui_font.h"
#include "services/resource_creation.h"
#include "ui/reading_state.h"
#include "model/resources.h"
#include "platform/path.h"
#include "theme/theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define TRY(call) do { int r_=(call); if(r_!=XUI_OK) return r_; } while(0)
struct xide_resource_view {
 xui_widget window,content,controls[XIDE_RES_CONTROL_COUNT],message,title,notice,footnote,pathLabel,backgrounds[3],placeholders[3];
 xide_ui_layout layout; float scale; int sync,selected,backgroundTheme,creating,layoutCreating,controlsAttempted,controlsResult;
 char *project,*manifest,*path,**paths,*original; size_t count;
 xvalue* projectBaseline; xide_resource_config config;
 xide_text_file disk; xfileinfo identity;
 xide_document_relocation* documents;
 xide_file_snapshot manifestDisk;
 char messageText[512];
};
static char* copy(const char* s) { char* p=malloc(strlen(s)+1); if(p) strcpy(p,s); return p; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* key) { return v ? xrtValueObjectGet(v,sv(key)) : NULL; }
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static void message(xide_resource_view* v,const char* s) { snprintf(v->messageText,sizeof(v->messageText),"%s",s); if(v->message) xuiLabelSetText(v->message,v->messageText); xgeRenderRequest(); }
static void openFailure(xide_app* a,xide_resource_view* v,const xide_model_error* e)
{
 message(v,e->message); snprintf(a->editorStatus,sizeof(a->editorStatus),"%.255s",e->message);
 if(a->hidden) { fprintf(stderr,"resource properties: %s\n",e->message); return; }
 xideMessageShow(a,"Windows 资源属性",e->message,XUI_MSGBOX_ICON_ERROR);
}
static int sameIdentity(const xfileinfo* a,const xfileinfo* b)
{ unsigned int required=XFILE_INFO_IDENTITY|XFILE_INFO_SIZE|XFILE_INFO_MODIFY_TIME; return (a->Available&required)==required && (b->Available&required)==required && a->Type==XFILE_TYPE_FILE && b->Type==XFILE_TYPE_FILE && a->Device==b->Device && a->Identity==b->Identity && a->Size==b->Size && a->Modified==b->Modified; }
static int readDisk(const char* path,xide_text_file* file,xfileinfo* identity,xide_model_error* e)
{
 xfileinfo after={0}; if(!xrtPathStat(path,false,identity) || identity->Type!=XFILE_TYPE_FILE || !xideTextFileRead(path,file,e) || !xrtPathStat(path,false,&after) || !sameIdentity(identity,&after)) return fail(e,XIDE_MODEL_CONFLICT,"资源文件在读取时变化、不可读取或不是普通文件，请重新打开表单。");
 return 1;
}
static void clearTarget(xide_resource_view* v)
{ xideResourceClear(&v->config); xideTextFileClear(&v->disk); xideDocumentsDestroyRelocation(v->documents); v->documents=NULL; free(v->path); free(v->original); v->path=v->original=NULL; }
static void clear(xide_resource_view* v)
{ clearTarget(v); xrtValueRelease(v->projectBaseline); v->projectBaseline=NULL; free(v->project); free(v->manifest); v->project=v->manifest=NULL; for(size_t i=0;i<v->count;++i) free(v->paths[i]); free(v->paths); v->paths=NULL; v->count=0; v->creating=0; }
static xvalue* draft(xide_resource_view* v,xide_model_error* e)
{
 xvalue* root=xrtValueDeepClone(v->config.root); if(!root) { fail(e,XIDE_MODEL_MEMORY,"无法准备资源草稿。"); return NULL; }
 const char* fields[9]; for(int i=0;i<9;++i) fields[i]=xuiEditGetText(v->controls[XIDE_RES_COMMENTS+i]);
 int okay=xideResourceSetVersion(root,xuiCheckBoxGetChecked(v->controls[XIDE_RES_VERSION_ENABLED]),fields,e) && xideResourceSetNumber(root,"language",xuiEditGetText(v->controls[XIDE_RES_LANGUAGE]),e) && xideResourceSetNumber(root,"codepage",xuiEditGetText(v->controls[XIDE_RES_CODEPAGE]),e);
 for(int i=0;okay && i<2;++i) okay=xideResourceSetSource(root,i,xuiComboBoxGetSelected(v->controls[i ? XIDE_RES_MANIFEST_MODE : XIDE_RES_ICON_MODE]),xuiTextEditGetText(v->controls[i ? XIDE_RES_MANIFEST : XIDE_RES_ICON]),e);
 if(okay) okay=xideResourceValidate(root,e);
 if(!okay) { xrtValueRelease(root); return NULL; } return root;
}
static int pending(xide_resource_view* v)
{ xide_model_error e={0}; xvalue* current=draft(v,&e); int changed=!current || !xrtValueEqual(current,v->config.root); xrtValueRelease(current); return changed; }
static void enabled(xide_resource_view* v)
{
 for(int i=0;i<9;++i) xuiWidgetSetEnabled(v->controls[XIDE_RES_COMMENTS+i],xuiCheckBoxGetChecked(v->controls[XIDE_RES_VERSION_ENABLED]));
 for(int i=0;i<2;++i) {
  int mode=xuiComboBoxGetSelected(v->controls[i ? XIDE_RES_MANIFEST_MODE : XIDE_RES_ICON_MODE]);
  xuiWidgetSetEnabled(v->controls[i ? XIDE_RES_MANIFEST : XIDE_RES_ICON],mode!=0);
  xuiWidgetSetEnabled(v->controls[i ? XIDE_RES_MANIFEST_BROWSE : XIDE_RES_ICON_BROWSE],mode==XIDE_RESOURCE_PATH);
 }
}
static void modeChanged(xui_widget w,int i,int val,void* user) { (void)w; (void)i; (void)val; enabled(((xide_app*)user)->resourceView); }
static void checkChanged(xui_widget w,int checked,void* user) { (void)w; (void)checked; enabled(((xide_app*)user)->resourceView); }
static void populate(xide_resource_view* v)
{
 xvalue* root=v->config.root,*version=get(root,"version"); v->sync=1;
 xuiCheckBoxSetChecked(v->controls[XIDE_RES_VERSION_ENABLED],version && xrtValueType(version)==XVALUE_OBJECT);
 for(int i=0;i<9;++i) xuiEditSetText(v->controls[XIDE_RES_COMMENTS+i],xideResourceVersion(root,i));
 char num[32]; snprintf(num,sizeof(num),"%u",xideResourceNumber(root,"language")); xuiEditSetText(v->controls[XIDE_RES_LANGUAGE],num); snprintf(num,sizeof(num),"%u",xideResourceNumber(root,"codepage")); xuiEditSetText(v->controls[XIDE_RES_CODEPAGE],num);
 for(int i=0;i<2;++i) { const char* payload=NULL; int mode=xideResourceSource(root,i,&payload); xuiComboBoxSetSelected(v->controls[i ? XIDE_RES_MANIFEST_MODE : XIDE_RES_ICON_MODE],mode); xuiTextEditSetText(v->controls[i ? XIDE_RES_MANIFEST : XIDE_RES_ICON],payload); }
 xuiTextEditSetText(v->controls[XIDE_RES_PREVIEW],""); v->sync=0; enabled(v);
}
static int load(xide_app* a,int index,xide_model_error* e)
{
 xide_resource_view* v=a->resourceView; if(index<0 || (size_t)index>=v->count) return fail(e,XIDE_MODEL_INVALID,"请选择资源配置。");
 xide_text_file disk={0}; xfileinfo identity={0}; xide_resource_config config={0};
 char* path=copy(v->paths[index]); if(!path || !readDisk(path,&disk,&identity,e)) { free(path); xideTextFileClear(&disk); return 0; }
 xide_document* d=xideDocumentsFind(&a->documents,path); const char* source=d ? xuiCodeEditGetText(d->editor) : disk.text;
 char* original=copy(source); xide_document_relocation* documents=xideDocumentsPrepareRelocation(&a->documents,NULL,NULL,e);
 if(!original || !documents || !xideResourceParse(&config,source,e)) { free(path); free(original); xideDocumentsDestroyRelocation(documents); xideTextFileClear(&disk); xideResourceClear(&config); return 0; }
 clearTarget(v); v->disk=disk; v->identity=identity; v->path=path; v->original=original; v->config=config; v->documents=documents; v->selected=index;
 v->sync=1; xuiComboBoxSetSelected(v->controls[XIDE_RES_CONFIG],index); v->sync=0; populate(v);
 message(v,d && d->dirty ? "正在编辑当前未保存资源正文。应用生成一次撤销编辑，随后 Ctrl+S 保存。取消保留原文。" : "应用生成一次可撤销的正文编辑，随后 Ctrl+S 保存。素材路径相对于此配置；取消不打开文档。"); return 1;
}
static void selected(xui_widget w,int index,int val,void* user)
{
 (void)w; (void)val; xide_app* a=user; xide_resource_view* v=a->resourceView; if(v->sync || index==v->selected) return;
 if(v->creating) {
  if(index<0 || index>1) return;
  const char* path=xuiEditGetText(v->controls[XIDE_RES_NEW_PATH]); size_t n=strlen(path); char* next=malloc(n+1);
  if(next) { strcpy(next,path); if(xidePathHasExtension(path,".json") || xidePathHasExtension(path,".xlon")) { strcpy(next+n-5,index ? ".xlon" : ".json"); xuiEditSetText(v->controls[XIDE_RES_NEW_PATH],next); } free(next); }
  v->selected=index; v->config.xson=index; xuiTextEditSetText(v->controls[XIDE_RES_PREVIEW],""); return;
 }
 xide_model_error e={0}; if(pending(v) || !load(a,index,&e)) { v->sync=1; xuiComboBoxSetSelected(v->controls[XIDE_RES_CONFIG],v->selected); v->sync=0; message(v,*e.message ? e.message : "当前字段尚未应用；请先应用或取消，再切换配置。输入已保留。"); }
}
static int newPath(xide_app* a,xide_model_error* e)
{
 xide_resource_view* v=a->resourceView; xide_project* p=xideWorkspaceFindProject(&a->workspaceModel,v->project);
 char* path=xideResourceCreationPath(p,xuiEditGetText(v->controls[XIDE_RES_NEW_PATH]),e); if(!path) return 0;
 if(!xidePathHasExtension(path,v->config.xson ? ".xlon" : ".json")) { free(path); return fail(e,XIDE_MODEL_INVALID,"文件扩展名须与所选 JSON / XLON 格式一致。"); }
 if(xideDocumentsFind(&a->documents,path)) { free(path); return fail(e,XIDE_MODEL_CONFLICT,"此路径已有打开文档，请使用新名称。"); }
 free(v->path); v->path=path; return 1;
}
static char* preview(xide_app* a,xvalue** out,xide_model_error* e)
{
 xide_resource_view* v=a->resourceView; if(v->creating && !newPath(a,e)) return NULL;
 xvalue* root=draft(v,e); if(!root) return NULL; char* body=xideResourceSerialize(&v->config,root,e);
 if(body && !xideResourceCheckAssets(root,v->path,e)) { xrtFree(body); body=NULL; }
 if(!body) { xrtValueRelease(root); return NULL; } if(out) *out=root; else xrtValueRelease(root); return body;
}
static int baseline(xide_app* a,xide_model_error* e)
{
 xide_resource_view* v=a->resourceView; xide_project* p=xideWorkspaceFindProject(&a->workspaceModel,v->project); xvalue* now=p ? xideProjectExport(p,e) : NULL;
 int same=p && !strcmp(p->manifest,v->manifest) && now && xrtValueEqual(now,v->projectBaseline); xrtValueRelease(now);
 if(!same || xideWorkspaceViewHasDraft(a)) return fail(e,XIDE_MODEL_CONFLICT,"项目或属性草稿已变化，请取消并重新打开资源表单。");
 if(!xideDocumentsValidateRelocation(v->documents,e)) return 0;
 if(v->creating) {
  xide_document* d=xideDocumentsFind(&a->documents,v->manifest); xide_file_snapshot disk={0};
  if((d && d->dirty) || !a->selectedProject || strcmp(a->selectedProject,v->project) || !xideFileSnapshotInspect(v->manifest,&disk,e) || !xideFileSnapshotEqual(&disk,&v->manifestDisk)) return fail(e,XIDE_MODEL_CONFLICT,"项目清单、选择或打开文档已变化，请重新打开资源创建表单。");
  return 1;
 }
 xide_text_file disk={0}; xfileinfo identity={0}; int okay=readDisk(v->path,&disk,&identity,e) && sameIdentity(&v->identity,&identity) && disk.rawLength==v->disk.rawLength && !memcmp(disk.raw,v->disk.raw,disk.rawLength); xideTextFileClear(&disk);
 return okay ? 1 : fail(e,XIDE_MODEL_CONFLICT,"资源文件在磁盘的身份或内容已变化；当前输入已保留，请先处理磁盘变化。");
}
static int createResource(xide_app* a,const char* body,xide_model_error* e)
{
 xide_resource_view* v=a->resourceView; xide_project* p=xideWorkspaceFindProject(&a->workspaceModel,v->project);
 xide_resource_plan* plan=xideResourcePlanPrepare(p,v->path,body,&v->manifestDisk,e); if(!plan) return 0;
 xvalue* reading=xideReadingCapture(a,NULL);
 if(!reading) { xideResourcePlanDestroy(plan); return fail(e,XIDE_MODEL_MEMORY,"无法保留工作空间与编辑器阅读状态。"); }
 int okay=xideResourcePlanCommit(p,plan,e);
 if(okay) {
  xideDialogSetOpen(v->window,0); int refresh=xideWorkspaceViewRefresh(a)==XUI_OK;
  if(refresh) { xideReadingRestore(a,reading,0); xideWorkbenchLayout(a); refresh=xuiLayout(a->context)==XUI_OK; if(refresh) xideReadingRestore(a,reading,1); }
  snprintf(a->editorStatus,sizeof(a->editorStatus),refresh ? "已创建资源配置并加入项目；请保存项目清单。" : "资源配置已创建并加入项目，但树刷新失败；请保存项目并重新打开。");
  message(v,a->editorStatus); xideActionsRefresh(a); if(a->activeDocument) xuiSetFocusWidget(a->context,a->activeDocument->editor); xgeRenderRequest();
 }
 xrtValueRelease(reading); xideResourcePlanDestroy(plan); return okay;
}
int xideResourceViewApply(xide_app* a,xide_model_error* e)
{
 xide_resource_view* v=a ? a->resourceView : NULL; if(!v || !xideResourceViewIsOpen(a)) return fail(e,XIDE_MODEL_INVALID,"没有打开资源表单。");
 if(!baseline(a,e)) return 0;
 xvalue* root=NULL; char* body=preview(a,&root,e); if(!body) return 0;
 if(v->creating) { int okay=createResource(a,body,e); xrtFree(body); xrtValueRelease(root); return okay; }
 if(xrtValueEqual(root,v->config.root)) { xrtFree(body); xrtValueRelease(root); xideDialogSetOpen(v->window,0); xideActionsRefresh(a); return 1; } xrtValueRelease(root);
 xide_document* d=xideDocumentsFind(&a->documents,v->path); int created=!d;
 if(!d) d=xideDocumentOpen(&a->documents,v->path,e);
 if(!d || strcmp(xuiCodeEditGetText(d->editor),v->original) || (created && (d->baseline.rawLength!=v->disk.rawLength || memcmp(d->baseline.raw,v->disk.raw,v->disk.rawLength)))) { if(d && created) xideDocumentClose(d,1,NULL); xrtFree(body); return fail(e,XIDE_MODEL_CONFLICT,"资源正文在应用前变化；输入已保留。"); }
 /* SDK modal focus isolation requires closing the window before focusing a dock editor. */
 xide_document* previous=a->activeDocument; xideDialogSetOpen(v->window,0);
 if(xideEditorShow(a,d)!=XUI_OK) { if(created) xideEditorClose(a,d,1,NULL); if(previous && previous->open) xideEditorShow(a,previous); xideDialogSetOpen(v->window,1); xuiSetFocusWidget(a->context,v->controls[XIDE_RES_APPLY]); xrtFree(body); return fail(e,XIDE_MODEL_IO,"无法显示资源文档，表单输入已保留。"); }
 xui_code_document core=xuiCodeEditGetDocument(d->editor); int result=xuiCodeDocumentReplace(core,0,xuiCodeDocumentGetLength(core),body); xrtFree(body);
 if(result!=XUI_OK) { xideDialogSetOpen(v->window,1); xuiSetFocusWidget(a->context,v->controls[XIDE_RES_APPLY]); return fail(e,XIDE_MODEL_IO,"无法将资源草稿应用为正文编辑，输入已保留。"); }
 xideDialogSetOpen(v->window,0); xideActionsRefresh(a); xuiSetFocusWidget(a->context,d->editor); xgeRenderRequest(); return 1;
}
static void assetSelected(xide_app* a,const char* path,void* user)
{
 if(!path || !a->resourceView) return;
 int manifest=(int)(intptr_t)user;xide_model_error e={0};
 int okay=!a->resourceView->creating || newPath(a,&e);char* base=okay ? xidePathDirectory(a->resourceView->path) : NULL;char* relative=base ? xidePathRelative(base,path) : NULL;
 if(relative) xuiTextEditSetText(a->resourceView->controls[manifest ? XIDE_RES_MANIFEST : XIDE_RES_ICON],relative);else if(!okay) message(a->resourceView,e.message);free(base);free(relative);
}
static void browse(xide_app* a,int manifest)
{
 xideFileDialogChoose(a,a->resourceView->window,XUI_FILE_DIALOG_MODE_OPEN_FILE,manifest ? "选择应用清单" : "选择 ICO 图标",manifest ? "应用清单|*.manifest;*.xml|所有文件|*.*" : "ICO 图标|*.ico|所有文件|*.*",a->resourceView->path,NULL,assetSelected,(void*)(intptr_t)manifest);
}
static void resourcePathSelected(xide_app* a,const char* path,void* user)
{ if(path && a->resourceView) { xuiEditSetText(a->resourceView->controls[XIDE_RES_NEW_PATH],path);xuiTextEditSetText(a->resourceView->controls[XIDE_RES_PREVIEW],""); } }
static void browseNew(xide_app* a)
{
 xide_resource_view* v=a->resourceView;xideFileDialogChoose(a,v->window,XUI_FILE_DIALOG_MODE_SAVE_FILE,"新资源配置的位置",v->config.xson ? "XLON 资源配置|*.xlon" : "JSON 资源配置|*.json",xuiEditGetText(v->controls[XIDE_RES_NEW_PATH]),v->config.xson ? "xlon" : "json",resourcePathSelected,a);
}
static void clicked(xui_widget w,void* user)
{
 xide_app* a=user; xide_resource_view* v=a->resourceView; int id=-1; for(int i=0;i<XIDE_RES_CONTROL_COUNT;++i) if(v->controls[i]==w) id=i;
 if(id==XIDE_RES_CANCEL) { xideDialogSetOpen(v->window,0); xideActionsRefresh(a); return; }
 if(id==XIDE_RES_NEW_BROWSE) { browseNew(a); return; }
 if(id==XIDE_RES_ICON_BROWSE || id==XIDE_RES_MANIFEST_BROWSE) { browse(a,id==XIDE_RES_MANIFEST_BROWSE); return; }
 xide_model_error e={0}; if(id==XIDE_RES_APPLY) { if(!xideResourceViewApply(a,&e)) message(v,e.message); }
 else if(id==XIDE_RES_VALIDATE) {
  char* body=(!v->creating || baseline(a,&e)) ? preview(a,NULL,&e) : NULL; xide_resource_plan* plan=NULL;
  if(body && v->creating) { xide_project* p=xideWorkspaceFindProject(&a->workspaceModel,v->project); plan=xideResourcePlanPrepare(p,v->path,body,&v->manifestDisk,&e); if(!plan) { xrtFree(body); body=NULL; } }
  xideResourcePlanDestroy(plan);
  if(body) { xuiTextEditSetText(v->controls[XIDE_RES_PREVIEW],body); xuiTextEditSetScroll(v->controls[XIDE_RES_PREVIEW],0,0); xrtFree(body); message(v,v->creating ? "预览尚未创建文件。点击创建后将加入资源引用；项目清单须正常保存。" : "字段、素材可读取性与 ICO 范围已检查。预览保留未知字段；XML 语义和实际编译由构建验证。"); } else { xuiTextEditSetText(v->controls[XIDE_RES_PREVIEW],""); message(v,e.message); }
 }
}
static int child(xide_app* a,xui_widget w,xui_rect_t r) { TRY(xuiWidgetSetRect(w,r)); return xuiWidgetAddChild(a->resourceView->content,w); }
static int label(xide_app* a,xui_widget* out,const char* s,xui_rect_t r)
{ xui_label_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=s; d.iWrapMode=XUI_TEXT_WRAP_WORD; xui_widget w; TRY(xuiLabelCreate(a->context,&w,&d)); TRY(child(a,w,r)); if(out) *out=w; return XUI_OK; }
static int input(xide_app* a,int id,const char* title,xui_rect_t r)
{ xui_input_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.iMaxLength=16*1024*1024; xui_widget* w=&a->resourceView->controls[id]; TRY(xuiInputCreate(a->context,w,&d)); TRY(xuiWidgetSetAccessibleName(*w,title)); return child(a,*w,r); }
static int button(xide_app* a,int id,const char* s,xui_rect_t r)
{ xui_button_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=s; xui_widget* w=&a->resourceView->controls[id]; TRY(xuiButtonCreate(a->context,w,&d)); TRY(xuiButtonSetClick(*w,clicked,a)); return child(a,*w,r); }
static int combo(xide_app* a,int id,const char** items,int count,xui_rect_t r,xui_combobox_select_proc proc)
{ xui_combobox_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.arrItems=items; d.iItemCount=count; xui_widget* w=&a->resourceView->controls[id]; TRY(xuiComboBoxCreate(a->context,w,&d)); TRY(xuiComboBoxSetSelect(*w,proc,a)); return child(a,*w,r); }
static void multilineChanged(xui_widget w,const char* text,void* user)
{
 xide_app* a=user; xide_resource_view* v=a->resourceView;
 xui_cache_policy_t c=xuiWidgetGetCachePolicy(w); int desired=*text ? XUI_CACHE_POLICY_AUTO : XUI_CACHE_POLICY_NONE;
 if(c.iPolicy!=desired) { c.iSize=sizeof(c); c.iPolicy=desired; xuiWidgetSetCachePolicy(w,&c); }
 const int ids[]={XIDE_RES_ICON,XIDE_RES_MANIFEST,XIDE_RES_PREVIEW};
 for(int i=0;i<3;++i) if(v->controls[ids[i]]==w) { if(xuiWidgetGetVisible(v->backgrounds[i])!=!*text) xuiWidgetSetVisible(v->backgrounds[i],!*text); if(xuiWidgetGetVisible(v->placeholders[i])!=!*text) xuiWidgetSetVisible(v->placeholders[i],!*text); }

}
static int multiline(xide_app* a,int id,int readonly,const char* name,xui_rect_t r)
{
 xide_resource_view* v=a->resourceView; int index=id==XIDE_RES_ICON ? 0 : id==XIDE_RES_MANIFEST ? 1 : 2;
 /* The pinned TextEdit raster cache cannot prepare an empty body. A native
    background and hint keep the empty input visible without inserting text. */
 xui_panel_desc_t panel={0}; panel.iSize=sizeof(panel); panel.fBorderWidth=1;
 TRY(xuiPanelCreate(a->context,&v->backgrounds[index],&panel)); TRY(child(a,v->backgrounds[index],r));
 TRY(label(a,&v->placeholders[index],readonly ? "点击检查素材并预览" : "在此输入路径、Base64 或清单文本",(xui_rect_t){r.fX+10,r.fY+8,r.fW-20,r.fH-16})); TRY(ideThemeSetTextRole(v->placeholders[index],IDE_MUTED));
 xui_text_edit_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.bReadonly=readonly; d.bWordWrap=1; d.iMaxLength=16*1024*1024; xui_widget* w=&v->controls[id];
 TRY(xuiTextEditCreate(a->context,w,&d)); xui_cache_policy_t c=xuiWidgetGetCachePolicy(*w); c.iSize=sizeof(c); c.iPolicy=XUI_CACHE_POLICY_NONE; TRY(xuiWidgetSetCachePolicy(*w,&c)); TRY(xuiTextEditSetChange(*w,multilineChanged,a)); TRY(xuiWidgetSetAccessibleName(*w,name)); return child(a,*w,r);
}
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->resourceView->controls[XIDE_RES_CANCEL],a); }
int xideResourceViewCreate(xide_app* a)
{
 xide_resource_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY;
 a->resourceView=v; v->backgroundTheme=-1; return XUI_OK;
}
static int createControls(xide_app* a)
{
 xide_resource_view* v=a->resourceView;
 TRY(xideDialogCreate(a,&v->window,"Windows 资源属性",864,1220,14)); TRY(xideDialogSetCancel(v->window,windowClosed,a)); v->content=xideDialogGetContentWidget(v->window); TRY(xuiWidgetAddChild(a->root,v->window)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
 TRY(label(a,&v->title,"Windows 资源属性",(xui_rect_t){0,0,850,30})); TRY(combo(a,XIDE_RES_CONFIG,NULL,0,(xui_rect_t){0,40,850,32},selected));
 TRY(label(a,&v->notice,"素材路径相对于资源配置。取消保留正文；关闭某资源段会移除该段及其扩展字段。",(xui_rect_t){0,82,850,48}));
 xui_checkbox_desc_t check={0}; check.iSize=sizeof(check); check.pFont=a->uiFont; check.sText="生成版本信息";
 TRY(xuiCheckBoxCreate(a->context,&v->controls[XIDE_RES_VERSION_ENABLED],&check)); TRY(xuiCheckBoxSetChange(v->controls[XIDE_RES_VERSION_ENABLED],checkChanged,a)); TRY(child(a,v->controls[XIDE_RES_VERSION_ENABLED],(xui_rect_t){0,134,240,32}));
 TRY(label(a,NULL,"language",(xui_rect_t){260,138,100,28})); TRY(input(a,XIDE_RES_LANGUAGE,"资源语言 ID",(xui_rect_t){362,134,178,32})); TRY(label(a,NULL,"codepage",(xui_rect_t){552,138,100,28})); TRY(input(a,XIDE_RES_CODEPAGE,"资源代码页",(xui_rect_t){656,134,194,32}));
 static const char* titles[]={"注释","公司名称","文件描述","文件版本","内部名称","版权声明","原始文件名","产品名称","产品版本"};
 for(int i=0;i<9;++i) { TRY(label(a,NULL,titles[i],(xui_rect_t){0,180+i*40,134,28})); TRY(input(a,XIDE_RES_COMMENTS+i,titles[i],(xui_rect_t){146,176+i*40,704,32})); }
 static const char* iconModes[]={"不生成图标","ICO 文件路径","ICO Base64"}; static const char* manifestModes[]={"不生成清单","清单文件路径","清单 Base64","清单文本"};
 TRY(label(a,NULL,"应用图标",(xui_rect_t){0,550,130,28})); TRY(combo(a,XIDE_RES_ICON_MODE,iconModes,3,(xui_rect_t){146,546,548,32},modeChanged)); TRY(button(a,XIDE_RES_ICON_BROWSE,"选择文件…",(xui_rect_t){708,546,142,32})); TRY(multiline(a,XIDE_RES_ICON,0,"图标路径或 Base64",(xui_rect_t){0,588,850,84}));
 TRY(label(a,NULL,"应用清单",(xui_rect_t){0,688,130,28})); TRY(combo(a,XIDE_RES_MANIFEST_MODE,manifestModes,4,(xui_rect_t){146,684,548,32},modeChanged)); TRY(button(a,XIDE_RES_MANIFEST_BROWSE,"选择文件…",(xui_rect_t){708,684,142,32})); TRY(multiline(a,XIDE_RES_MANIFEST,0,"清单路径、Base64 或文本",(xui_rect_t){0,726,850,138}));
 TRY(button(a,XIDE_RES_VALIDATE,"检查素材并预览",(xui_rect_t){0,880,240,32})); TRY(label(a,&v->footnote,"应用后正文会成为未保存编辑；Ctrl+S 后参与构建。",(xui_rect_t){256,880,594,32})); TRY(multiline(a,XIDE_RES_PREVIEW,1,"无损资源配置预览，只读",(xui_rect_t){0,924,850,180}));
 TRY(label(a,&v->message,"",(xui_rect_t){0,1114,850,60})); TRY(button(a,XIDE_RES_APPLY,"应用到文档",(xui_rect_t){506,1186,194,32})); TRY(button(a,XIDE_RES_CANCEL,"取消",(xui_rect_t){714,1186,136,32}));
 TRY(label(a,&v->pathLabel,"新配置路径",(xui_rect_t){0,138,130,28})); TRY(input(a,XIDE_RES_NEW_PATH,"新资源配置路径",(xui_rect_t){146,134,548,32})); TRY(button(a,XIDE_RES_NEW_BROWSE,"选择位置…",(xui_rect_t){708,134,142,32})); v->layoutCreating=-1; return xideUiLayoutCapture(&v->layout,v->content);
}
static int ensureControls(xide_app* a)
{
 xide_resource_view* v=a->resourceView;
 if(!v->controlsAttempted) {
  /* The native panel creates several owned children. Defer this optional form
     until it is requested, and bind current font metrics only to this subtree. */
  v->controlsAttempted=1; v->controlsResult=createControls(a);
  if(v->controlsResult==XUI_OK) v->controlsResult=xideUiFontBindWidget(a,v->window);
 }
 if(v->controlsResult==XUI_OK) return 1;
 if(v->window) xideDialogSetOpen(v->window,0);
 xide_model_error e={0}; fail(&e,XIDE_MODEL_IO,"资源表单初始化失败；请重启 IDE 后重试。"); openFailure(a,v,&e); return 0;
}
static void setMode(xide_resource_view* v)
{
 xuiWidgetSetVisible(v->pathLabel,v->creating); xuiWidgetSetVisible(v->controls[XIDE_RES_NEW_PATH],v->creating); xuiWidgetSetVisible(v->controls[XIDE_RES_NEW_BROWSE],v->creating);
 xuiLabelSetText(v->title,v->creating ? "新建 Windows 资源配置" : "Windows 资源属性"); xuiWindowSetTitle(v->window,v->creating ? "新建 Windows 资源配置" : "Windows 资源属性");
 xuiLabelSetText(v->notice,v->creating ? "选择格式和新文件路径。素材路径相对于新配置；父目录须存在。取消不创建文件。" : "素材路径相对于资源配置。取消保留正文；关闭某资源段会移除该段及其扩展字段。");
 xuiLabelSetText(v->footnote,v->creating ? "创建保存配置文件并加入项目引用；请保存项目清单。" : "应用后正文会成为未保存编辑；Ctrl+S 后参与构建。");
 xuiButtonSetText(v->controls[XIDE_RES_APPLY],v->creating ? "创建并加入项目" : "应用到文档");
}
int xideResourceViewOpen(xide_app* a)
{
 if(xideResourceViewIsOpen(a)) return 1;
 xide_resource_view* v=a ? a->resourceView : NULL; xide_project* p=a ? xideWorkspaceFindProject(&a->workspaceModel,a->selectedProject) : NULL;
 if(!v || !p || !p->available || !p->resources.count || p->resources.count>INT_MAX || xideActionsHasModal(a) || xideWorkspaceViewHasDraft(a)) return 0;
 if(!ensureControls(a)) return 0;
 clear(v); v->project=copy(p->id); v->manifest=copy(p->manifest); v->projectBaseline=xideProjectExport(p,NULL); v->paths=calloc(p->resources.count,sizeof(*v->paths));
 setMode(v);
 if(!v->project || !v->manifest || !v->projectBaseline || !v->paths) { clear(v); return 0; }
 int index=0; const xide_file* f=xideWorkspaceViewSelectedFile(a);
 for(size_t i=0;i<p->resources.count;++i) { v->paths[i]=copy(p->resources.items[i]); if(!v->paths[i]) { v->count=i; clear(v); return 0; } ++v->count; if((f && xidePathEqual(f->path,v->paths[i])) || (!f && a->activeDocument && a->activeDocument->path && xidePathEqual(a->activeDocument->path,v->paths[i]))) index=(int)i; }
 v->sync=1; xuiComboBoxSetItems(v->controls[XIDE_RES_CONFIG],(const char**)v->paths,(int)v->count); v->sync=0;
 xide_model_error e={0}; if(!load(a,index,&e)) { openFailure(a,v,&e); clear(v); return 0; }
 xideResourceViewLayout(a); xuiScrollViewSetOffset(xideDialogGetScrollViewWidget(v->window),0,0); xideDialogSetOpen(v->window,1); xideActionsRefresh(a); xuiSetFocusWidget(a->context,v->controls[XIDE_RES_CONFIG]); xgeRenderRequest(); return 1;
}
int xideResourceViewOpenNew(xide_app* a)
{
 xide_resource_view* v=a ? a->resourceView : NULL; xide_project* p=a ? xideWorkspaceFindProject(&a->workspaceModel,a->selectedProject) : NULL;
 if(!v || !p || !p->available || xideActionsHasModal(a) || xideWorkspaceViewHasDraft(a)) return 0;
 xide_document* manifest=xideDocumentsFind(&a->documents,p->manifest); xide_model_error e={0};
 if(!ensureControls(a)) return 0;
 if(manifest && manifest->dirty) { fail(&e,XIDE_MODEL_CONFLICT,"请先保存或处理打开的项目清单正文。"); openFailure(a,v,&e); return 0; }
 clear(v); v->creating=1; v->project=copy(p->id); v->manifest=copy(p->manifest); v->projectBaseline=xideProjectExport(p,&e); v->documents=xideDocumentsPrepareRelocation(&a->documents,NULL,NULL,&e);
 char* path=xideResourceCreationPath(p,"app.res.json",&e); v->path=path;
 int okay=v->project && v->manifest && v->projectBaseline && v->documents && path && xideFileSnapshotInspect(p->manifest,&v->manifestDisk,&e) && xideResourceParse(&v->config,"{\"format\":\"xlang-windows-resource-1\",\"language\":1033,\"codepage\":1200}",&e);
 const char* fields[]={"","",p->description,"1.0.0.0","","","",p->name,"1.0.0.0"};
 if(okay) okay=xideResourceSetVersion(v->config.root,1,fields,&e);
 if(!okay) { if(!e.code) fail(&e,XIDE_MODEL_MEMORY,"无法准备资源创建表单。"); openFailure(a,v,&e); clear(v); return 0; }
 static const char* formats[]={"JSON (.json)","XLON (.xlon)"}; v->sync=1; xuiComboBoxSetItems(v->controls[XIDE_RES_CONFIG],formats,2); xuiComboBoxSetSelected(v->controls[XIDE_RES_CONFIG],0); v->selected=0; xuiEditSetText(v->controls[XIDE_RES_NEW_PATH],"app.res.json"); v->sync=0; populate(v); setMode(v);
 message(v,"预览不会写入文件；创建不会覆盖目标，原有 C/xlang 文档及启动项目保持。");
 xideResourceViewLayout(a); xuiScrollViewSetOffset(xideDialogGetScrollViewWidget(v->window),0,0); xideDialogSetOpen(v->window,1); xideActionsRefresh(a); xuiSetFocusWidget(a->context,v->controls[XIDE_RES_NEW_PATH]); xgeRenderRequest(); return 1;
}
int xideResourceViewIsOpen(xide_app* a) { return a && a->resourceView && a->resourceView->window && a->resourceView->controlsResult==XUI_OK && xideDialogIsOpen(a->resourceView->window); }
void xideResourceViewLayout(xide_app* a)
{
 xide_resource_view* v=a->resourceView; if(!v || !v->window || v->controlsResult!=XUI_OK) return;
 const int ids[]={XIDE_RES_ICON,XIDE_RES_MANIFEST,XIDE_RES_PREVIEW};
 for(int i=0;i<3;++i) if(v->controls[ids[i]]) multilineChanged(v->controls[ids[i]],xuiTextEditGetText(v->controls[ids[i]]),a);
 if(v->backgroundTheme!=a->theme) { for(int i=0;i<3;++i) if(v->backgrounds[i]) { xuiPanelSetBackgroundColor(v->backgrounds[i],ideThemeColor(a->theme,IDE_FIELD)); xuiPanelSetClientColor(v->backgrounds[i],ideThemeColor(a->theme,IDE_FIELD)); xuiPanelSetBorder(v->backgrounds[i],1,ideThemeColor(a->theme,IDE_BORDER)); } v->backgroundTheme=a->theme; }
 int width=a->viewportWidth-48,height=a->viewportHeight-48; if(width<200) width=200; if(height<200) height=200; xideDialogSetMaxSize(v->window,(float)width,(float)height); float scale=xideUiFontScale(a);
 int contentHeight=v->creating ? 1272 : 1220;
 if(v->scale!=scale || v->layoutCreating!=v->creating) {
  for(size_t i=0;i<v->layout.count;++i) { xui_widget w=v->layout.widgets[i]; xui_rect_t r=v->layout.rectangles[i]; if(v->creating && r.fY>=134 && w!=v->pathLabel && w!=v->controls[XIDE_RES_NEW_PATH] && w!=v->controls[XIDE_RES_NEW_BROWSE]) r.fY+=52; if(w==v->controls[XIDE_RES_APPLY] && !v->creating) { r.fX=550; r.fW=150; } xuiWidgetSetRect(w,xideUiRect(a,r)); }
  xideDialogSetContentSize(v->window,xideUiHeight(a,864),xideUiHeight(a,contentHeight)); v->scale=scale; v->layoutCreating=v->creating;
 }
 
}
void xideResourceViewDestroy(xide_app* a) { if(a->resourceView) { clear(a->resourceView); free(a->resourceView); a->resourceView=NULL; } }
xui_widget xideResourceViewWindow(xide_app* a) { return a->resourceView ? a->resourceView->window : NULL; }
xui_widget xideResourceViewControl(xide_app* a,int i) { return a->resourceView && i>=0 && i<XIDE_RES_CONTROL_COUNT ? a->resourceView->controls[i] : NULL; }
const char* xideResourceViewMessage(xide_app* a) { return a->resourceView ? a->resourceView->messageText : ""; }
