#define _WIN32_WINNT 0x0601
#include "ui/import_view.h"
#include "services/legacy_import.h"
#include "services/session.h"
#include "services/file_sync.h"
#include "services/ui_font.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#define TRY(call) do { int r_=(call); if(r_!=XUI_OK) return r_; } while(0)
#define PREVIEW_LIMIT (8u*1024u*1024u)
struct xide_import_view {
 xui_widget window,content,controls[XIDE_IMPORT_CONTROL_COUNT],message;
 xide_ui_layout layout; float scale; int sync,published;
 xide_legacy_import* plan; xvalue* baseline; xide_document_relocation* documents;
 char *source,*destination,*appliedEntry,*reportText; char** projectTitles; size_t projectTitleCount;
 char messageText[1024];
};
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* get(const xvalue* v,const char* k) { return v ? xrtValueObjectGet(v,sv(k)) : NULL; }
static const char* text(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && !memchr(s.Data,0,s.Size) ? s.Data : ""; }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* out=malloc(n); if(out) memcpy(out,s,n); return out; }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s ? s : ""))); }
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static void message(xide_import_view* v,const char* s) { snprintf(v->messageText,sizeof(v->messageText),"%s",s); xuiLabelSetText(v->message,v->messageText); xgeRenderRequest(); }
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
static void enabled(xui_widget w,int value)
{
 xuiWidgetSetEnabled(w,value); uint32_t input=xuiWidgetGetInputState(w),state=0;
 const uint32_t states[]={XUI_WIDGET_STATE_DISABLED,XUI_WIDGET_STATE_ACTIVE,XUI_WIDGET_STATE_HOVER,XUI_WIDGET_STATE_FOCUS};
 for(size_t i=0;i<sizeof(states)/sizeof(*states);++i) if(input&states[i]) { state=states[i]; break; }
 xuiWidgetSetStateId(w,state);
}
static void state(xide_import_view* v)
{
 int project=xuiComboBoxGetSelected(v->controls[XIDE_IMPORT_SELECTED_PROJECT]);
 int supported=v->plan && project>=0 && xideLegacyImportProjectSupported(v->plan,(size_t)project);
 const int edits[]={XIDE_IMPORT_SOURCE,XIDE_IMPORT_BROWSE_SOURCE,XIDE_IMPORT_DESTINATION,XIDE_IMPORT_BROWSE_DESTINATION,XIDE_IMPORT_ANALYZE};
 for(size_t i=0;i<sizeof(edits)/sizeof(*edits);++i) enabled(v->controls[edits[i]],!v->published);
 enabled(v->controls[XIDE_IMPORT_SELECTED_PROJECT],v->plan && xideLegacyImportProjectCount(v->plan)>0);
 const int choices[]={XIDE_IMPORT_CANDIDATES,XIDE_IMPORT_ENTRY,XIDE_IMPORT_BROWSE_ENTRY,XIDE_IMPORT_SET_ENTRY,XIDE_IMPORT_KIND};
 for(size_t i=0;i<sizeof(choices)/sizeof(*choices);++i) enabled(v->controls[choices[i]],supported && !v->published);
 enabled(v->controls[XIDE_IMPORT_STARTUP],v->plan && !v->published);
 enabled(v->controls[XIDE_IMPORT_REVIEWED],v->plan && !v->published);
 enabled(v->controls[XIDE_IMPORT_OPEN_AFTER],!v->published);
 enabled(v->controls[XIDE_IMPORT_RESTORE_STATE],xuiCheckBoxGetChecked(v->controls[XIDE_IMPORT_OPEN_AFTER]) && !v->published);
 enabled(v->controls[XIDE_IMPORT_APPLY],v->published || (v->plan && xideLegacyImportReady(v->plan) && xuiCheckBoxGetChecked(v->controls[XIDE_IMPORT_REVIEWED])));
 xuiButtonSetText(v->controls[XIDE_IMPORT_APPLY],v->published ? "重试打开" : "导入");
}
static void clear(xide_import_view* v)
{
 v->sync=1; xuiComboBoxSetItems(v->controls[XIDE_IMPORT_SELECTED_PROJECT],NULL,0); xuiComboBoxSetItems(v->controls[XIDE_IMPORT_STARTUP],NULL,0); xuiComboBoxSetItems(v->controls[XIDE_IMPORT_CANDIDATES],NULL,0);
 xuiEditSetText(v->controls[XIDE_IMPORT_ENTRY],""); xuiTextEditSetText(v->controls[XIDE_IMPORT_PREVIEW],""); xuiCheckBoxSetChecked(v->controls[XIDE_IMPORT_REVIEWED],0); v->sync=0;
 for(size_t i=0;v->projectTitles && i<v->projectTitleCount;++i) { free(v->projectTitles[i]); } free(v->projectTitles); v->projectTitles=NULL; v->projectTitleCount=0;
 xideLegacyImportDestroy(v->plan); v->plan=NULL; xrtValueRelease(v->baseline); v->baseline=NULL; xideDocumentsDestroyRelocation(v->documents); v->documents=NULL;
 free(v->source); v->source=NULL; free(v->destination); v->destination=NULL; free(v->appliedEntry); v->appliedEntry=NULL; free(v->reportText); v->reportText=NULL; v->published=0;
}
static int freeze(xide_app* a,xide_model_error* e)
{
 xvalue* b=baseline(a); xide_document_relocation* d=b ? xideDocumentsPrepareRelocation(&a->documents,NULL,NULL,e) : NULL;
 if(!b || !d) { xrtValueRelease(b); xideDocumentsDestroyRelocation(d); return fail(e,XIDE_MODEL_MEMORY,"无法保存当前工作空间和文档基线。"); }
 xide_import_view* v=a->importView; xrtValueRelease(v->baseline); xideDocumentsDestroyRelocation(v->documents); v->baseline=b; v->documents=d; return 1;
}
static int validBaseline(xide_app* a,xide_model_error* e)
{
 xide_import_view* v=a->importView; xvalue* now=baseline(a); int same=now && v->baseline && xrtValueEqual(now,v->baseline); xrtValueRelease(now);
 if(!same || xideWorkspaceViewHasDraft(a)) return fail(e,XIDE_MODEL_CONFLICT,"当前工作空间或项目属性已改变，请关闭并重新分析导入。");
 return xideDocumentsValidateRelocation(v->documents,e);
}
static int reportAppend(char* out,size_t* length,const char* format,...)
{
 if(*length>=PREVIEW_LIMIT-1024) return 0;
 va_list args; va_start(args,format); int n=vsnprintf(out+*length,PREVIEW_LIMIT-*length,format,args); va_end(args);
 if(n<0 || (size_t)n>=PREVIEW_LIMIT-*length) { out[*length]=0; return 0; } *length+=(size_t)n; return 1;
}
static int reportBuild(xide_import_view* v)
{
 xvalue* r=xrtJsonParse(sv(xideLegacyImportReportJson(v->plan))); char* out=malloc(PREVIEW_LIMIT); size_t length=0;
 if(!r || !out) { xrtValueRelease(r); free(out); return 0; }
 out[0]=0; int okay=reportAppend(out,&length,"旧工程迁移报告\n\n来源：%s\n新目录：%s\n\n生成新清单并引用原始文件，原工程文件不会被改写。请核对入口、输出类型、启动项目、构建和运行语义。\n\n项目：\n",text(get(r,"source")),text(get(r,"destination")));
 const xvalue* projects=get(r,"projects");
 for(size_t i=0;okay && i<xrtValueCount(projects);++i) {
  const xvalue* p=xrtValueArrayGet(projects,i); bool supported=false; xrtValueGetBool(get(p,"supported"),&supported);
  okay=reportAppend(out,&length,"%zu. %s\n   新清单：%s\n   %s\n   入口：%s\n   类型：%s\n\n",i+1,text(get(p,"source")),text(get(p,"manifest")),supported ? "可转换" : "保留原引用，需手工处理",text(get(p,"entry")),text(get(p,"kind")));
 }
 const xvalue* issues=get(r,"issues"); if(okay) okay=reportAppend(out,&length,"需核对事项（%zu 项）：\n",xrtValueCount(issues));
 for(size_t i=0;okay && i<xrtValueCount(issues);++i) {
  const xvalue* item=xrtValueArrayGet(issues,i);
  okay=reportAppend(out,&length,"\n%zu. [%s] %s\n   %s\n",i+1,text(get(item,"code")),text(get(item,"message")),text(get(item,"source")));
 }
 if(!okay) { snprintf(out+length,PREVIEW_LIMIT-length,"\n报告超过预览限制；完整报告将保存在新目录 report.json。\n"); }
 else reportAppend(out,&length,"\n原始字段、未转换字段和旧状态完整保存在生成清单、report.json 和 session-hints.json 中。\n选择导入后打开时，可以应用旧打开文件和展开状态；已有原生会话受到保护。\n");
 free(v->reportText); v->reportText=out; xrtValueRelease(r); return 1;
}
static void preview(xide_import_view* v)
{
 int choice=xuiComboBoxGetSelected(v->controls[XIDE_IMPORT_PREVIEW_KIND]),project=xuiComboBoxGetSelected(v->controls[XIDE_IMPORT_SELECTED_PROJECT]); const char* body="请先选择旧清单与新目录，然后分析。";
 if(v->plan) {
  if(choice==1) body=xideLegacyImportWorkspaceJson(v->plan);
  else if(choice==2) body=project>=0 ? xideLegacyImportProjectJson(v->plan,(size_t)project) : NULL;
  else if(choice==3) body=xideLegacyImportReportJson(v->plan);
  else if(choice==4) body=xideLegacyImportSessionHintsJson(v->plan);
  else body=v->reportText;
 }
 if(!body) body="该项目未转换，原引用和原始数据已保留，请查看迁移报告。";
 if(strlen(body)>=PREVIEW_LIMIT) body="完整 JSON 超过 8 MiB 预览限制。迁移报告提供摘要，导入后可打开生成的 report.json 查看完整原始数据。";
 xuiTextEditSetText(v->controls[XIDE_IMPORT_PREVIEW],body); xuiTextEditSetScroll(v->controls[XIDE_IMPORT_PREVIEW],0,0);
}
static int refresh(xide_import_view* v,int resetReview)
{
 if(!v->plan) { state(v); preview(v); return 1; }
 int project=xuiComboBoxGetSelected(v->controls[XIDE_IMPORT_SELECTED_PROJECT]); size_t count=xideLegacyImportProjectCount(v->plan); if(project<0 || (size_t)project>=count) project=count ? 0 : -1;
 v->sync=1; xuiComboBoxSetSelected(v->controls[XIDE_IMPORT_SELECTED_PROJECT],project);
 size_t n=project>=0 ? xideLegacyImportEntryCount(v->plan,(size_t)project) : 0; const char** candidates=n ? malloc(n*sizeof(*candidates)) : NULL;
 if(n && !candidates) { v->sync=0; return 0; }
 for(size_t i=0;i<n;++i) candidates[i]=xideLegacyImportEntryAt(v->plan,(size_t)project,i);
 int okay=xuiComboBoxSetItems(v->controls[XIDE_IMPORT_CANDIDATES],candidates,(int)n)==XUI_OK; free(candidates);
 const char* json=project>=0 ? xideLegacyImportProjectJson(v->plan,(size_t)project) : NULL; xvalue* p=json ? xrtJsonParse(sv(json)) : NULL;
 const char* entry=text(get(p,"entry")); char* absolute=*entry ? xidePathResolve(xideLegacyImportDirectory(v->plan),entry) : copy("");
 if(!absolute) okay=0; else { free(v->appliedEntry); v->appliedEntry=absolute; xuiEditSetText(v->controls[XIDE_IMPORT_ENTRY],absolute); }
 int candidate=-1; for(size_t i=0;v->appliedEntry && i<n;++i) if(xidePathEqual(v->appliedEntry,xideLegacyImportEntryAt(v->plan,(size_t)project,i))) candidate=(int)i;
 xuiComboBoxSetSelected(v->controls[XIDE_IMPORT_CANDIDATES],candidate);
 const char* kind=text(get(p,"kind")); xuiComboBoxSetSelected(v->controls[XIDE_IMPORT_KIND],!strcmp(kind,"exe") ? 1 : !strcmp(kind,"dll") ? 2 : 0); xrtValueRelease(p);
 xvalue* w=xrtJsonParse(sv(xideLegacyImportWorkspaceJson(v->plan))); const char* startup=text(get(w,"startupProject")); int selected=0;
 for(size_t i=0;i<count;++i) if(!strcmp(startup,xideLegacyImportProjectId(v->plan,i))) selected=(int)i+1;
 xuiComboBoxSetSelected(v->controls[XIDE_IMPORT_STARTUP],selected); xrtValueRelease(w);
 if(resetReview) xuiCheckBoxSetChecked(v->controls[XIDE_IMPORT_REVIEWED],0);
 v->sync=0; if(!reportBuild(v)) okay=0; preview(v); state(v); xgeRenderRequest(); return okay;
}
int xideImportViewAnalyze(xide_app* a,xide_model_error* e)
{
 xide_import_view* v=a ? a->importView : NULL; if(!v || !xideImportViewIsOpen(a) || v->published) return fail(e,XIDE_MODEL_INVALID,"当前不能重新分析导入。");
 char *source=copy(xuiEditGetText(v->controls[XIDE_IMPORT_SOURCE])),*destination=copy(xuiEditGetText(v->controls[XIDE_IMPORT_DESTINATION]));
 if(!source || !destination) { free(source); free(destination); return fail(e,XIDE_MODEL_MEMORY,"无法保存导入输入。"); }
 clear(v); xide_legacy_import* plan=xideLegacyImportPrepare(source,destination,xidePathHasExtension(source,".xlp") ? XIDE_IMPORT_PROJECT : XIDE_IMPORT_WORKSPACE,e); free(source); free(destination);
 if(!plan) { state(v); return 0; } v->plan=plan;
 xvalue* report=xrtJsonParse(sv(xideLegacyImportReportJson(plan))); v->source=report ? copy(text(get(report,"source"))) : NULL; v->destination=copy(xideLegacyImportDirectory(plan)); xrtValueRelease(report);
 size_t count=xideLegacyImportProjectCount(plan); v->projectTitles=calloc(count+1,sizeof(*v->projectTitles)); v->projectTitleCount=count+1;
 int okay=v->source && v->destination && v->projectTitles;
 if(okay) { v->projectTitles[0]=copy("不设置启动项目"); okay=v->projectTitles[0]!=NULL; }
 for(size_t i=0;okay && i<count;++i) {
  const char* json=xideLegacyImportProjectJson(plan,i); xvalue* p=json ? xrtJsonParse(sv(json)) : NULL; const char* name=text(get(get(p,"ide"),"name"));
  if(!*name) { name=xidePathFilename(xideLegacyImportProjectSource(plan,i)); } size_t size=strlen(name)+48; v->projectTitles[i+1]=malloc(size);
  if(v->projectTitles[i+1]) snprintf(v->projectTitles[i+1],size,"%s%s",name,xideLegacyImportProjectSupported(plan,i) ? "" : " [未转换]"); else okay=0; xrtValueRelease(p);
 }
 if(okay) {
  v->sync=1; xuiEditSetText(v->controls[XIDE_IMPORT_SOURCE],v->source); xuiEditSetText(v->controls[XIDE_IMPORT_DESTINATION],v->destination);
  okay=xuiComboBoxSetItems(v->controls[XIDE_IMPORT_SELECTED_PROJECT],(const char**)&v->projectTitles[1],(int)count)==XUI_OK && xuiComboBoxSetItems(v->controls[XIDE_IMPORT_STARTUP],(const char**)v->projectTitles,(int)count+1)==XUI_OK; v->sync=0;
 }
 if(!okay || !freeze(a,e) || !refresh(v,1)) { clear(v); state(v); if(!e || !e->code) fail(e,XIDE_MODEL_MEMORY,"无法建立导入预览，请重试分析。"); return 0; }
 message(v,xideLegacyImportReady(plan) ? "请阅读迁移报告并确认。所有原始文件保持原位置。" : "请选择每个待转换项目的入口和输出类型，再阅读并确认报告。"); return 1;
}
int xideImportViewSetEntry(xide_app* a,xide_model_error* e)
{
 xide_import_view* v=a ? a->importView : NULL; int i=v ? xuiComboBoxGetSelected(v->controls[XIDE_IMPORT_SELECTED_PROJECT]) : -1;
 if(!v || !xideImportViewIsOpen(a) || v->published || !v->plan || i<0) return fail(e,XIDE_MODEL_INVALID,"请先分析并选择可转换项目。");
 if(!xideLegacyImportSetEntry(v->plan,(size_t)i,xuiEditGetText(v->controls[XIDE_IMPORT_ENTRY]),e)) return 0;
 return refresh(v,1) ? 1 : fail(e,XIDE_MODEL_MEMORY,"无法更新入口预览。");
}
int xideImportViewApply(xide_app* a,xide_model_error* e)
{
 xide_import_view* v=a ? a->importView : NULL;
 if(!v || !xideImportViewIsOpen(a) || !v->plan) return fail(e,XIDE_MODEL_INVALID,"请先分析旧清单。");
 if(!validBaseline(a,e)) return 0;
 if(!v->published && (!xideLegacyImportReady(v->plan) || !xuiCheckBoxGetChecked(v->controls[XIDE_IMPORT_REVIEWED]))) return fail(e,XIDE_MODEL_INVALID,"请完成入口与类型选择，并确认已经阅读迁移报告。");
 if(!v->published && (strcmp(v->source,xuiEditGetText(v->controls[XIDE_IMPORT_SOURCE])) || strcmp(v->destination,xuiEditGetText(v->controls[XIDE_IMPORT_DESTINATION])))) return fail(e,XIDE_MODEL_CONFLICT,"来源或目录已改变，请重新分析。");
 if(!v->published && v->appliedEntry && strcmp(v->appliedEntry,xuiEditGetText(v->controls[XIDE_IMPORT_ENTRY]))) return fail(e,XIDE_MODEL_CONFLICT,"入口输入尚未确认，请先点击确认入口。");
 int open=xuiCheckBoxGetChecked(v->controls[XIDE_IMPORT_OPEN_AFTER]);
 xvalue* hints=open && xuiCheckBoxGetChecked(v->controls[XIDE_IMPORT_RESTORE_STATE]) ? xrtJsonParse(sv(xideLegacyImportSessionHintsJson(v->plan))) : NULL;
 if(open && xuiCheckBoxGetChecked(v->controls[XIDE_IMPORT_RESTORE_STATE]) && !hints) return fail(e,XIDE_MODEL_MEMORY,"无法准备旧会话状态。");
 if(open) {
  if(!xideWorkspaceQuitRequest(a)) { xrtValueRelease(hints); return fail(e,XIDE_MODEL_CONFLICT,"当前工程切换已取消，导入目录和文档保持原状态。"); }
  /* Native Save decisions may legitimately update the baseline. */
  if(!freeze(a,e) || !xideSessionSave(a,0,e)) { xrtValueRelease(hints); return 0; }
 }
 if(!v->published) { if(!xideLegacyImportCommit(v->plan,1,e)) { xrtValueRelease(hints); return 0; } v->published=1; state(v); }
 if(!open) {
  snprintf(a->editorStatus,sizeof(a->editorStatus),"已生成导入清单：%s",xideLegacyImportDirectory(v->plan)); xuiStatusBarSetItemText(a->statusBar,0,a->editorStatus);
  xideDialogSetOpen(v->window,0); clear(v); state(v); xideActionsRefresh(a); xgeRenderRequest(); return 1;
 }
 xideDialogSetOpen(v->window,0); int previous=a->sessionSwitchConfirmed; a->sessionSwitchConfirmed=1;
 int opened=xideWorkspaceOpen(a,xideLegacyImportWorkspacePath(v->plan),0,e); a->sessionSwitchConfirmed=previous;
 if(!opened) {
  xrtValueRelease(hints); xideDialogSetOpen(v->window,1); xide_model_error ignored={0}; freeze(a,&ignored);
  char detail[1024]; snprintf(detail,sizeof(detail),"新清单已生成，打开失败：%.600s。生成目录保留，可重试打开或关闭窗口。",e ? e->message : "请检查清单"); message(v,detail); state(v); xideActionsRefresh(a); return 0;
 }
 int hadHints=hints!=NULL; xide_legacy_session_result restored={0}; xide_model_error stateError={0}; int restoredOkay=!hadHints || xideSessionApplyLegacyHints(a,hints,&restored,&stateError); xrtValueRelease(hints);
 if(!hadHints && !xideSessionSave(a,0,&stateError)) restoredOkay=0;
 xide_model_error watchError={0}; int watched=xideFileSyncRebind(a,&watchError);
 if(!restoredOkay) snprintf(a->editorStatus,sizeof(a->editorStatus),"已导入并打开；旧状态/会话：%.170s",stateError.message);
 else if(!watched) snprintf(a->editorStatus,sizeof(a->editorStatus),"已导入并打开；文件协调：%.170s",watchError.message);
 else snprintf(a->editorStatus,sizeof(a->editorStatus),"已导入并打开，恢复 %zu 个文件，%zu 个引用不可用。",restored.opened,restored.unavailable);
 xuiStatusBarSetItemText(a->statusBar,0,a->editorStatus); clear(v); state(v); xideActionsRefresh(a); xideWorkbenchLayout(a); xgeRenderRequest(); return 1;
}
static int changed(xui_widget w,const xui_edit_event_t* event,void* user)
{
 xide_app* a=user; xide_import_view* v=a->importView;
 if(event->iType==XUI_EDIT_EVENT_TEXT_CHANGED && !v->sync && !v->published && v->plan) {
  if((w==v->controls[XIDE_IMPORT_SOURCE] && strcmp(xuiEditGetText(w),v->source)) || (w==v->controls[XIDE_IMPORT_DESTINATION] && strcmp(xuiEditGetText(w),v->destination))) { clear(v); message(v,"路径已改变，请重新分析。"); }
  state(v);
 }
 return XUI_OK;
}
static void checked(xui_widget w,int value,void* user)
{ (void)w; (void)value; xide_app* a=user; if(!a->importView->sync) state(a->importView); xgeRenderRequest(); }
static void chosen(xui_widget w,int index,int value,void* user)
{
 (void)value; xide_app* a=user; xide_import_view* v=a->importView; if(v->sync) return;
 xide_model_error e={0}; int okay=1,project=xuiComboBoxGetSelected(v->controls[XIDE_IMPORT_SELECTED_PROJECT]);
 if(w==v->controls[XIDE_IMPORT_PREVIEW_KIND]) preview(v);
 else if(w==v->controls[XIDE_IMPORT_SELECTED_PROJECT]) okay=refresh(v,0);
 else if(v->plan && !v->published) {
  if(w==v->controls[XIDE_IMPORT_CANDIDATES] && project>=0 && index>=0) {
   const char* path=xideLegacyImportEntryAt(v->plan,(size_t)project,(size_t)index); okay=path && xideLegacyImportSetEntry(v->plan,(size_t)project,path,&e);
  } else if(w==v->controls[XIDE_IMPORT_KIND] && project>=0) okay=index>0 && xideLegacyImportSetKind(v->plan,(size_t)project,index==1 ? "exe" : "dll",&e);
  else if(w==v->controls[XIDE_IMPORT_STARTUP]) okay=xideLegacyImportSetStartup(v->plan,index>0 ? xideLegacyImportProjectId(v->plan,(size_t)index-1) : "",&e);
  refresh(v,okay);
 }
 if(!okay) message(v,e.message[0] ? e.message : "无法更新选择，请选择有效项。"); else if(w!=v->controls[XIDE_IMPORT_PREVIEW_KIND] && w!=v->controls[XIDE_IMPORT_SELECTED_PROJECT]) message(v,"导入选项已更新，请重新核对并确认迁移报告。");
 xgeRenderRequest();
}
static void importSelected(xide_app* a,const char* path,void* user)
{
 if(!path || !a->importView) return;
 int control=(int)(intptr_t)user;
 char* selected=control==XIDE_IMPORT_BROWSE_DESTINATION ? xidePathResolve(path,"legacy-import") : copy(path);
 if(selected) { int target=control==XIDE_IMPORT_BROWSE_SOURCE ? XIDE_IMPORT_SOURCE : control==XIDE_IMPORT_BROWSE_DESTINATION ? XIDE_IMPORT_DESTINATION : XIDE_IMPORT_ENTRY;xuiEditSetText(a->importView->controls[target],selected);free(selected); }
}
static void browse(xide_app* a,int control)
{
 int folder=control==XIDE_IMPORT_BROWSE_DESTINATION;
 const char* title=folder ? "选择新导入目录的父目录" : control==XIDE_IMPORT_BROWSE_SOURCE ? "选择旧工作空间或项目" : "选择 xlang 入口";
 const char* filter=control==XIDE_IMPORT_BROWSE_SOURCE ? "旧工程|*.xlw;*.xlp|所有文件|*.*" : "xlang 源文件|*.xl|所有文件|*.*";
 xideFileDialogChoose(a,a->importView->window,folder ? XUI_FILE_DIALOG_MODE_SELECT_FOLDER : XUI_FILE_DIALOG_MODE_OPEN_FILE,title,folder ? NULL : filter,NULL,NULL,importSelected,(void*)(intptr_t)control);
}
static void clicked(xui_widget w,void* user)
{
 xide_app* a=user; xide_import_view* v=a->importView; xide_model_error e={0};
 if(w==v->controls[XIDE_IMPORT_CANCEL]) { xideDialogSetOpen(v->window,0); clear(v); state(v); xideActionsRefresh(a); }
 else if(w==v->controls[XIDE_IMPORT_ANALYZE]) { if(!xideImportViewAnalyze(a,&e)) message(v,e.message); }
 else if(w==v->controls[XIDE_IMPORT_SET_ENTRY]) { if(!xideImportViewSetEntry(a,&e)) message(v,e.message); }
 else if(w==v->controls[XIDE_IMPORT_APPLY]) {
  if(!xideImportViewApply(a,&e)) { if(!v->published) message(v,e.message); else { char detail[1024]; snprintf(detail,sizeof(detail),"新清单已生成：%.600s。目录保留，可重试打开或关闭窗口。",e.message); message(v,detail); } }
 }
 else for(int i=0;i<XIDE_IMPORT_CONTROL_COUNT;++i) if(w==v->controls[i]) { browse(a,i); break; }
 xgeRenderRequest();
}
static int label(xide_app* a,xui_widget* out,const char* s,xui_rect_t r)
{ xui_widget w=NULL; xui_label_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=s; d.iWrapMode=XUI_TEXT_WRAP_WORD; TRY(xuiLabelCreate(a->context,&w,&d)); TRY(xuiWidgetSetRect(w,r)); TRY(xuiWidgetAddChild(a->importView->content,w)); if(out) *out=w; return XUI_OK; }
static int input(xide_app* a,int i,const char* name,xui_rect_t r)
{ xui_input_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; TRY(xuiInputCreate(a->context,&a->importView->controls[i],&d)); xui_widget w=a->importView->controls[i]; TRY(xuiWidgetSetRect(w,r)); TRY(xuiWidgetSetAccessibleName(w,name)); TRY(xuiEditSetEvent(w,changed,a)); return xuiWidgetAddChild(a->importView->content,w); }
static int button(xide_app* a,int i,const char* s,xui_rect_t r)
{ xui_button_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=s; TRY(xuiButtonCreate(a->context,&a->importView->controls[i],&d)); xui_widget w=a->importView->controls[i]; TRY(xuiWidgetSetRect(w,r)); TRY(xuiButtonSetClick(w,clicked,a)); return xuiWidgetAddChild(a->importView->content,w); }
static int combo(xide_app* a,int i,const char* name,const char** items,int count,xui_rect_t r)
{ xui_combobox_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.arrItems=items; d.iItemCount=count; TRY(xuiComboBoxCreate(a->context,&a->importView->controls[i],&d)); xui_widget w=a->importView->controls[i]; TRY(xuiWidgetSetRect(w,r)); TRY(xuiWidgetSetAccessibleName(w,name)); TRY(xuiComboBoxSetSelect(w,chosen,a)); return xuiWidgetAddChild(a->importView->content,w); }
static int check(xide_app* a,int i,const char* s,xui_rect_t r)
{ xui_checkbox_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=s; d.fIndicatorSize=18; d.fGap=8; d.bUseBuiltinAtlas=1; TRY(xuiCheckBoxCreate(a->context,&a->importView->controls[i],&d)); xui_widget w=a->importView->controls[i]; TRY(xuiWidgetSetRect(w,r)); TRY(xuiCheckBoxSetChange(w,checked,a)); return xuiWidgetAddChild(a->importView->content,w); }
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->importView->controls[XIDE_IMPORT_CANCEL],a); }
int xideImportViewCreate(xide_app* a)
{
 xide_import_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY; a->importView=v;
 TRY(xideDialogCreate(a,&v->window,"导入旧工作空间或项目",864,940,14)); TRY(xideDialogSetCancel(v->window,windowClosed,a)); v->content=xideDialogGetContentWidget(v->window); TRY(xuiWidgetAddChild(a->root,v->window)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
 TRY(label(a,NULL,"导入旧工作空间或项目",(xui_rect_t){0,0,850,30})); TRY(label(a,NULL,"旧清单",(xui_rect_t){0,44,96,28}));
 TRY(input(a,XIDE_IMPORT_SOURCE,"旧 .xlw 或 .xlp 清单",(xui_rect_t){104,40,622,32})); TRY(button(a,XIDE_IMPORT_BROWSE_SOURCE,"选择文件…",(xui_rect_t){736,40,114,32}));
 TRY(label(a,NULL,"新目录",(xui_rect_t){0,86,96,28})); TRY(input(a,XIDE_IMPORT_DESTINATION,"将生成的新目录，不可已存在",(xui_rect_t){104,82,622,32})); TRY(button(a,XIDE_IMPORT_BROWSE_DESTINATION,"选择位置…",(xui_rect_t){736,82,114,32}));
 TRY(label(a,NULL,"生成新工程清单，源文件仍引用原位置。选择不存在的新目录。",(xui_rect_t){0,124,720,40})); TRY(button(a,XIDE_IMPORT_ANALYZE,"分析旧清单",(xui_rect_t){736,122,114,32}));
 TRY(label(a,NULL,"项目",(xui_rect_t){0,174,96,28})); TRY(combo(a,XIDE_IMPORT_SELECTED_PROJECT,"选择待转换项目",NULL,0,(xui_rect_t){104,170,746,32}));
 TRY(label(a,NULL,"入口候选",(xui_rect_t){0,216,96,28})); TRY(combo(a,XIDE_IMPORT_CANDIDATES,"旧清单中 xlang 入口候选",NULL,0,(xui_rect_t){104,212,746,32}));
 TRY(label(a,NULL,"指定入口",(xui_rect_t){0,258,96,28})); TRY(input(a,XIDE_IMPORT_ENTRY,"所选项目入口，可引用清单外的 .xl",(xui_rect_t){104,254,486,32})); TRY(button(a,XIDE_IMPORT_BROWSE_ENTRY,"选择文件…",(xui_rect_t){600,254,114,32})); TRY(button(a,XIDE_IMPORT_SET_ENTRY,"确认入口",(xui_rect_t){724,254,126,32}));
 static const char* kinds[]={"请选择","exe","dll"}; TRY(label(a,NULL,"输出类型",(xui_rect_t){0,300,96,28})); TRY(combo(a,XIDE_IMPORT_KIND,"输出类型",kinds,3,(xui_rect_t){104,296,230,32}));
 TRY(label(a,NULL,"启动项目",(xui_rect_t){348,300,96,28})); TRY(combo(a,XIDE_IMPORT_STARTUP,"启动项目，不按旧 Open 顺序推断",NULL,0,(xui_rect_t){452,296,398,32}));
 static const char* previews[]={"迁移报告","工作空间清单","当前项目清单","原始报告 JSON","会话提示"}; TRY(label(a,NULL,"预览内容",(xui_rect_t){0,342,96,28})); TRY(combo(a,XIDE_IMPORT_PREVIEW_KIND,"迁移预览内容",previews,5,(xui_rect_t){104,338,746,32}));
 xui_text_edit_desc_t t={0}; t.iSize=sizeof(t); t.pFont=a->uiFont; t.sText="请先选择旧清单与新目录，然后分析。"; t.bReadonly=1; t.bWordWrap=1; t.iMaxLength=PREVIEW_LIMIT;
 TRY(xuiTextEditCreate(a->context,&v->controls[XIDE_IMPORT_PREVIEW],&t)); TRY(xuiWidgetSetRect(v->controls[XIDE_IMPORT_PREVIEW],(xui_rect_t){0,382,850,320})); TRY(xuiWidgetSetAccessibleName(v->controls[XIDE_IMPORT_PREVIEW],"单栏迁移报告和生成清单，只读")); TRY(xuiWidgetAddChild(v->content,v->controls[XIDE_IMPORT_PREVIEW]));
 TRY(check(a,XIDE_IMPORT_REVIEWED,"已阅读迁移报告，确认入口、类型及未转换字段",(xui_rect_t){0,714,850,32}));
 TRY(check(a,XIDE_IMPORT_OPEN_AFTER,"导入后打开工作空间",(xui_rect_t){0,750,850,32})); TRY(check(a,XIDE_IMPORT_RESTORE_STATE,"应用旧打开文件和树展开状态（已有会话受保护）",(xui_rect_t){0,786,850,32}));
 TRY(label(a,&v->message,"",(xui_rect_t){0,828,850,68})); TRY(button(a,XIDE_IMPORT_APPLY,"导入",(xui_rect_t){602,906,120,32})); TRY(button(a,XIDE_IMPORT_CANCEL,"关闭",(xui_rect_t){734,906,116,32}));
 TRY(xuiComboBoxSetSelected(v->controls[XIDE_IMPORT_PREVIEW_KIND],0)); TRY(xuiCheckBoxSetChecked(v->controls[XIDE_IMPORT_OPEN_AFTER],1)); TRY(xuiCheckBoxSetChecked(v->controls[XIDE_IMPORT_RESTORE_STATE],1)); state(v);
 return xideUiLayoutCapture(&v->layout,v->content);
}
int xideImportViewOpen(xide_app* a)
{
 if(xideImportViewIsOpen(a)) return 1;
 xide_import_view* v=a ? a->importView : NULL; if(!v || !a->workspaceView || xideWorkspaceViewHasDraft(a) || xideActionsHasModal(a)) return 0;
 clear(v); xuiCheckBoxSetChecked(v->controls[XIDE_IMPORT_OPEN_AFTER],1); xuiCheckBoxSetChecked(v->controls[XIDE_IMPORT_RESTORE_STATE],1); state(v); preview(v);
 message(v,"选择旧清单和不存在的新目录，然后分析。导入会保留原始数据和未转换引用。"); xideImportViewLayout(a); xideDialogSetScroll(v->window,0,0); xideDialogSetOpen(v->window,1); xuiSetFocusWidget(a->context,v->controls[XIDE_IMPORT_SOURCE]); xideActionsRefresh(a); xgeRenderRequest(); return 1;
}
int xideImportViewIsOpen(xide_app* a) { return a && a->importView && xideDialogIsOpen(a->importView->window); }
void xideImportViewLayout(xide_app* a)
{
 xide_import_view* v=a->importView; if(!v) return; int width=a->viewportWidth-48,height=a->viewportHeight-48; if(width<200) width=200; if(height<200) height=200;
 xideDialogSetMaxSize(v->window,(float)width,(float)height); float scale=xideUiFontScale(a);
 if(v->scale!=scale) { xideUiLayoutApply(a,&v->layout,1); xideDialogSetContentSize(v->window,xideUiHeight(a,864),xideUiHeight(a,940)); v->scale=scale; }
 
}
void xideImportViewDestroy(xide_app* a) { if(a->importView) { clear(a->importView); free(a->importView); a->importView=NULL; } }
xui_widget xideImportViewWindow(xide_app* a) { return a && a->importView ? a->importView->window : NULL; }
xui_widget xideImportViewControl(xide_app* a,int i) { return a && a->importView && i>=0 && i<XIDE_IMPORT_CONTROL_COUNT ? a->importView->controls[i] : NULL; }
const char* xideImportViewMessage(xide_app* a) { return a && a->importView ? a->importView->messageText : ""; }
int xideImportViewPublished(xide_app* a) { return a && a->importView && a->importView->published; }
