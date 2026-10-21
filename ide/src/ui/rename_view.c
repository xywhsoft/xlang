#include "ui/resource_view.h"
#include "ui/rename_view.h"
#include "ui/workspace_view.h"
#include "ui/editor_view.h"
#include "services/ui_font.h"
#include "services/file_rebind.h"
#include "services/resource_rebind.h"
#include "services/project_migration.h"
#include "services/file_sync.h"
#include "services/session.h"
#include "ui/reading_state.h"
#include "platform/file_rename.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TRY(call) do { int result_=(call); if(result_!=XUI_OK) return result_; } while(0)
struct xide_rename_view {
 xui_widget window,content,controls[XIDE_RENAME_CONTROL_COUNT],message,title,originalLabel,nameLabel,targetLabel;
 xide_ui_layout layout; float scale; xvalue* model;
 uint64_t document; uint32_t version; char *original,*documentPath,*language,*target; xide_file_snapshot disk; int fromTree; unsigned char* raw; size_t rawLength;
 int dirty,languageOverride; char messageText[512];
 int mode; char *projectId,*projectSessionBefore;
 xide_project_migration* migrationBaseline;
 xide_document_relocation* documentBaseline;
 xide_resource_rebind* resources;
 int pendingResources;
};
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s ? s : ""))); }
static int fail(xide_model_error* e,int code,const char* text) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",text); } return 0; }
static void message(xide_rename_view* v,const char* text) { snprintf(v->messageText,sizeof(v->messageText),"%s",text); xuiLabelSetText(v->message,v->messageText); xgeRenderRequest(); }
static void checkpointRenamed(xide_app* a)
{
 xide_model_error e={0};
 if(!xideSessionSave(a,0,&e)) {
  const char* warning="文件已改名；会话检查点写入失败。请先处理会话存储；原检查点保留。";
  message(a->renameView,warning); if(a->statusBar) xuiStatusBarSetItemText(a->statusBar,1,warning);
  fprintf(stderr,"rename session checkpoint: %s\n",e.message);
 }
}
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
static xide_document* current(xide_app* a)
{ for(size_t i=0;i<a->documents.count;++i) { xide_document* d=a->documents.items[i]; if(d->open && d->id==a->renameView->document) return d; } return NULL; }
static void clear(xide_rename_view* v)
{ xrtValueRelease(v->model); v->model=NULL; free(v->original); v->original=NULL; free(v->documentPath); v->documentPath=NULL; free(v->language); v->language=NULL; free(v->target); v->target=NULL; free(v->raw); v->raw=NULL; v->rawLength=0;
 xideProjectMigrationDestroy(v->migrationBaseline); v->migrationBaseline=NULL;
 xideDocumentsDestroyRelocation(v->documentBaseline); v->documentBaseline=NULL;
 xideResourceRebindDestroy(v->resources); v->resources=NULL; v->pendingResources=0;
 free(v->projectId); v->projectId=NULL; free(v->projectSessionBefore); v->projectSessionBefore=NULL; v->mode=0; }
int xideRenameViewCanOpenFile(xide_app* a,const char* path)
{
 if(!a || !a->renameView || !path || xideWorkspaceViewHasDraft(a) || !xideFileRenameSourceValid(path,NULL)) return 0;
 if(a->workspaceModel.manifest && xidePathEqual(path,a->workspaceModel.manifest)) return 0;
 for(size_t i=0;i<a->workspaceModel.projectCount;++i) if(xidePathEqual(path,a->workspaceModel.projects[i].manifest)) return 0;
 return 1;
}
int xideRenameViewCanOpen(xide_app* a)
{
 xide_document* d=a ? a->activeDocument : NULL;
 if(!a || !a->renameView || !d || !d->open || !d->path || xideWorkspaceViewHasDraft(a)) return 0;
 if(a->workspaceModel.manifest && xidePathEqual(d->path,a->workspaceModel.manifest)) return 0;
 for(size_t i=0;i<a->workspaceModel.projectCount;++i) if(xidePathEqual(d->path,a->workspaceModel.projects[i].manifest)) return 0;
 return 1;
}

int xideRenameViewCanMigrateProject(xide_app* a)
{
 xide_project* p=a ? xideWorkspaceFindProject(&a->workspaceModel,a->selectedProject) : NULL;
 return a && a->renameView && p && p->available && !xideWorkspaceViewHasDraft(a);
}
static int refreshMigration(xide_app* a)
{
 xide_rename_view* v=a->renameView; xide_model_error e={0}; const char* text=xuiEditGetText(v->controls[XIDE_RENAME_NAME]);
 char* base=xidePathDirectory(v->original); char* target=base && text && *text ? xidePathResolveLexical(base,text) : NULL; free(base);
 char* parent=target ? xidePathDirectory(target) : NULL;
 int okay=target && parent && xidePathIsDirectory(parent) && xideFileRenameTargetValid(target,&e) && (v->mode==2 || xidePathHasExtension(target,".xlp"));
 free(parent); free(v->target); v->target=okay ? target : NULL; if(!okay) free(target);
 xuiEditSetText(v->controls[XIDE_RENAME_TARGET],v->target ? v->target : ""); xuiWidgetSetEnabled(v->controls[XIDE_RENAME_APPLY],okay);
 xuiWidgetSetStateId(v->controls[XIDE_RENAME_APPLY],okay ? 0 : XUI_WIDGET_STATE_DISABLED);
 message(v,okay ? (v->mode==2 ? "移动整个项目目录及其内容。已打开文件和工程引用将更新，未保存正文保留。完成后请保存工作空间和项目。" : "只移动项目清单；源代码仍在原处。已打开清单和工程引用将更新，完成后请保存工作空间和项目。") : e.message[0] ? e.message : "请输入目标完整路径（也可相对原位置）；目标父目录必须存在，项目清单须使用 .xlp。");
 return okay;
}
static char* mapDocument(const char* path,void* user) { return xideProjectMigrationMapPath(user,path); }
static int sameString(const char* a,const char* b) { return (!a && !b) || (a && b && !strcmp(a,b)); }
static int inside(const char* path,const char* root) { size_t n=strlen(root); return path && strlen(path)>=n && !memcmp(path,root,n) && (!path[n] || path[n]=='/'); }
static int cleanManifests(xide_app* a,const xide_project_migration* plan,xide_model_error* e)
{
 if(!xideProjectMigrationPathCount(plan)) return 1;
 for(size_t i=0;i<a->documents.count;++i) {
  xide_document* d=a->documents.items[i]; if(!d->open || !d->path || !d->dirty) continue;
  int affected=a->workspaceModel.manifest && xidePathEqual(d->path,a->workspaceModel.manifest);
  for(size_t j=0;!affected && j<xideProjectMigrationProjectCount(plan);++j) affected=xidePathEqual(d->path,xideProjectMigrationProjectAt(plan,j)->manifest);
  if(affected) return fail(e,XIDE_MODEL_CONFLICT,"受影响工程清单存在未保存正文，请先保存或重载清单，再打开迁移窗口。");
 }
 return 1;
}
static int applyMigration(xide_app* a,xide_model_error* e)
{
 xide_rename_view* v=a->renameView;
 if(!sameString(v->projectId,a->selectedProject) || !sameString(v->projectSessionBefore,a->projectSessionPath) || xideWorkspaceViewHasDraft(a) || !xideDocumentsValidateRelocation(v->documentBaseline,e))
  return fail(e,XIDE_MODEL_CONFLICT,"所选项目、文档或工程状态已改变，请取消后重新打开迁移窗口。");
 if(!refreshMigration(a)) return fail(e,XIDE_MODEL_INVALID,v->messageText);
 xide_project_migration* plan=xideProjectMigrationRetarget(v->migrationBaseline,v->target,e); if(!plan) return 0;
 xide_document_relocation* documents=NULL; xvalue* reading=NULL; char* sessionPath=NULL; int okay=0;
 if(!cleanManifests(a,plan,e)) goto done;
 if(v->mode==2 && a->sessionRoot && xideProjectMigrationPathCount(plan) && inside(xideSessionDirectory(a),v->original)) { fail(e,XIDE_MODEL_CONFLICT,"当前会话存储位于待迁移目录内，请改用目录外的会话存储后再迁移。"); goto done; }
 documents=xideDocumentsPrepareRelocation(&a->documents,mapDocument,plan,e); if(!documents) goto done;
 reading=xideReadingCapture(a,NULL); if(!reading) { fail(e,XIDE_MODEL_MEMORY,"无法预分配工程阅读状态。"); goto done; }
 if(a->projectSessionPath) { sessionPath=xideProjectMigrationMapPath(plan,a->projectSessionPath); if(!sessionPath) { fail(e,XIDE_MODEL_MEMORY,"无法准备独立项目会话绑定。"); goto done; } }
 /* Keep the window open through checkpointing. Watch coordination is paused;
    no UI dispatch occurs between validating, physical commit and all bindings. */
 if(!xideSessionBeforeProjectMigration(a,e) || !xideSessionSave(a,0,e) || !xideDocumentsValidateRelocation(v->documentBaseline,e) || !xideDocumentsValidateRelocation(documents,e)) goto done;
 if(!xideSessionPrepareProjectMigration(a,plan,e)) goto done;
 if(!xideProjectMigrationCommit(plan,e)) goto done;
 okay=1;
 for(size_t i=0;i<xideProjectMigrationPathCount(plan);++i) {
  xideTasksInvalidateInput(a->tasks,xideProjectMigrationPathBefore(plan,i)); xideTasksInvalidateInput(a->tasks,xideProjectMigrationPathAfter(plan,i));
 }
 for(size_t i=0;i<xideProjectMigrationProjectCount(plan);++i) xideTasksInvalidateInput(a->tasks,xideProjectMigrationProjectAt(plan,i)->manifest);
 int relocated=xideProjectMigrationWorkspaceMoves(plan) || !sameString(a->projectSessionPath,sessionPath);
 if(a->projectSessionPath) { free(a->projectSessionPath); a->projectSessionPath=sessionPath; sessionPath=NULL; }
 xideDocumentsApplyRelocation(documents);
 xide_model_error post={0}; if(!xideFileSyncRebind(a,&post)) { fprintf(stderr,"migration watch rebind: %s\n",post.message); xideFileSyncRefresh(a); }
 int refreshed=xideWorkspaceViewRefresh(a); if(refreshed!=XUI_OK) a->frameError=refreshed;
 if(refreshed==XUI_OK) { xideReadingRestore(a,reading,0); xideWorkbenchLayout(a); int laid=xuiLayout(a->context); if(laid==XUI_OK) xideReadingRestore(a,reading,1); else a->frameError=laid; }
 if(relocated) xideSessionRelocate(a); else if(!xideSessionSave(a,0,&post)) fprintf(stderr,"migration session checkpoint: %s\n",post.message);
 xideDialogSetOpen(v->window,0); clear(v); xuiSetFocusWidget(a->context,a->workspace);
done:
 if(!okay) xideSessionCancelProjectMigration(a);
 free(sessionPath); xrtValueRelease(reading); xideDocumentsDestroyRelocation(documents); xideProjectMigrationDestroy(plan);
 if(!okay) { message(v,e && e->message[0] ? e->message : "迁移失败，当前输入和原文件保留。"); xuiSetFocusWidget(a->context,v->controls[XIDE_RENAME_NAME]); }
 xideActionsRefresh(a); xgeRenderRequest(); return okay;
}

int xideRenameViewRefresh(xide_app* a)
{
 xide_rename_view* v=a->renameView; if(!v || !v->original) return 0;
 if(v->pendingResources) return 1;
 if(v->mode) return refreshMigration(a);
 const char* name=xuiEditGetText(v->controls[XIDE_RENAME_NAME]); xide_model_error e={0}; char* folder=xidePathDirectory(v->original);
 char* target=folder && name && *name && !strpbrk(name,"/\\:") ? xidePathResolveLexical(folder,name) : NULL; free(folder);
 int okay=target && xideFileRenameTargetValid(name,&e); xide_file_rebind* plan=NULL;
 if(okay) okay=xideFileRebindPrepare(&a->workspaceModel,v->original,target,&plan,&e);
 xideFileRebindDestroy(plan); free(v->target); v->target=okay ? target : NULL; if(!okay) free(target);
 xuiEditSetText(v->controls[XIDE_RENAME_TARGET],v->target ? v->target : ""); xuiWidgetSetEnabled(v->controls[XIDE_RENAME_APPLY],okay);
 xuiWidgetSetStateId(v->controls[XIDE_RENAME_APPLY],okay ? 0 : XUI_WIDGET_STATE_DISABLED);
 char info[512]; snprintf(info,sizeof(info),"修改磁盘文件名及工程引用；%zu 份资源配置的素材路径将作为可撤销的未保存编辑更新。完成后请保存文档和工程。",xideResourceRebindCount(v->resources));
 message(v,okay ? info : e.message[0] ? e.message : "请输入同一目录内的文件名，不包含目录或驱动器。"); return okay;
}
static int changed(xui_widget w,const xui_edit_event_t* e,void* user)
{ (void)w; if(e->iType==XUI_EDIT_EVENT_TEXT_CHANGED) xideRenameViewRefresh(user); return XUI_OK; }
int xideRenameViewApply(xide_app* a,xide_model_error* e)
{
 xide_rename_view* v=a->renameView; if(!v || !xideRenameViewIsOpen(a)) return fail(e,XIDE_MODEL_INVALID,"重命名窗口尚未打开。");
 if(v->pendingResources) {
  if(!xideResourceRebindApply(v->resources,e)) { checkpointRenamed(a); return 0; }
  xide_model_error journal={0}; if(!xideSessionFinishRename(a,v->resources,&journal)) message(v,journal.message);
  xideDialogSetOpen(v->window,0); clear(v); checkpointRenamed(a); xideActionsRefresh(a); xgeRenderRequest(); return 1;
 }
 if(v->mode) return applyMigration(a,e);
 xide_document* d=current(a); xvalue* now=baseline(a); int same=now && v->model && xrtValueEqual(now,v->model); xrtValueRelease(now);
 int documentSame=!v->document || (d && d->path && !strcmp(d->path,v->documentPath) && xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))==v->version && d->dirty==v->dirty && d->languageOverride==v->languageOverride && !strcmp(d->language,v->language) && d->baseline.rawLength==v->rawLength && (!v->rawLength || !memcmp(d->baseline.raw,v->raw,v->rawLength)));
 const xide_file* selected=v->fromTree ? xideWorkspaceViewSelectedFile(a) : NULL;
 int selectionSame=!v->fromTree || (selected && !strcmp(selected->path,v->original));
 if(!same || !documentSame || !selectionSame || (!v->document && xideDocumentsFind(&a->documents,v->original)) || xideWorkspaceViewHasDraft(a)) return fail(e,XIDE_MODEL_CONFLICT,"文件选择、文档或工程状态已改变。当前文件名保留，请取消后重新打开窗口。");
 if(!xideRenameViewRefresh(a)) return fail(e,XIDE_MODEL_INVALID,v->messageText);
 if(!strcmp(v->original,v->target)) { int tree=v->fromTree; xideDialogSetOpen(v->window,0); clear(v); xuiSetFocusWidget(a->context,tree ? a->workspace : d->editor); xideActionsRefresh(a); xgeRenderRequest(); return 1; }
 if(!xideResourceRebindRetarget(v->resources,v->target,e) || !xideResourceRebindValidate(v->resources,e)) return 0;
 xvalue* reading=xideReadingCapture(a,NULL); if(!reading) return fail(e,XIDE_MODEL_MEMORY,"无法准备改名前阅读状态。");
 xide_document* active=a->activeDocument;
 int okay=0,moved=0,fromTree=v->fromTree;
 /* No event dispatch while temporarily removing the modal focus barrier to
    attach resource tabs. Model/disk guards were captured above. */
 xideDialogSetOpen(v->window,0);
 int prepared=xideResourceRebindOpen(v->resources,e);
 for(size_t i=0;prepared && i<xideResourceRebindCount(v->resources);++i)
  if(xideEditorShow(a,xideResourceRebindDocument(v->resources,i))!=XUI_OK) prepared=fail(e,XIDE_MODEL_MEMORY,"无法预打开待更新资源文档。");
 if(prepared) prepared=xideResourceRebindValidate(v->resources,e);
 /* Preserve every open dirty/untitled body before the irreversible disk move.
    Restore the user's active tab before capture; temporary resource attachment
    must not become the crash-recovery active document. */
 if(prepared && active && active->open) prepared=xideEditorShow(a,active)==XUI_OK || fail(e,XIDE_MODEL_MEMORY,"无法恢复改名前的活动文档。");
 if(prepared) prepared=xideSessionSave(a,0,e);
 if(prepared) prepared=xideResourceRebindValidate(v->resources,e);
 if(prepared) prepared=xideSessionPrepareRename(a,v->original,v->target,v->resources,e);
 if(!prepared) goto resourceFailure;
 /* No event dispatch occurs between closing this modal, preflight and commit.
    Other modal windows remain blocked by the common beforeSave callback. */
 xideDialogSetOpen(v->window,0);
 if(d) okay=xideDocumentRenameSource(d,v->original,v->target,fromTree ? &v->disk : NULL,e);
 else if(!strcmp(v->original,v->target)) okay=1;
 else if(xideDocumentsFind(&a->documents,v->target)) fail(e,XIDE_MODEL_CONFLICT,"目标作为另一个文档打开，请先处理目标文件。");
 else {
  okay=xideEditorPreparePathChange(a,v->original,v->target,e) && xideFileRenameChecked(v->original,v->target,NULL,&v->disk,e);
  if(okay) { xideTasksInvalidateInput(a->tasks,v->original); xideTasksInvalidateInput(a->tasks,v->target); }
  xideEditorFinishPathChange(a,v->target,okay);
 }
 if(okay) {
  moved=1;
  int resourcesApplied=xideResourceRebindApply(v->resources,e);
  xide_model_error journal={0}; if(!xideSessionFinishRename(a,v->resources,&journal)) { fprintf(stderr,"rename journal checkpoint: %s\n",journal.message); if(a->statusBar) xuiStatusBarSetItemText(a->statusBar,1,journal.message); }
  if(!resourcesApplied) {
   v->pendingResources=1; xuiWidgetSetEnabled(v->controls[XIDE_RENAME_NAME],0); xuiWidgetSetEnabled(v->controls[XIDE_RENAME_CANCEL],0); xuiButtonSetText(v->controls[XIDE_RENAME_APPLY],"完成资源更新");
   message(v,e && e->message[0] ? e->message : "文件已改名，资源配置尚待更新。请重试完成资源更新。"); okay=0;
  } else clear(v);
 } else {
resourceFailure:
  xideSessionCancelPreparedRename(a);
  /* Roll back pre-opened clean tabs only. Existing dirty resource text is intact. */
  for(size_t i=0;i<xideResourceRebindCount(v->resources);++i) { xide_document* resource=xideResourceRebindDocument(v->resources,i); if(xideResourceRebindOpened(v->resources,i) && resource && resource->window>=0) xuiDockPanelHideWindow(a->dock,resource->window); }
  xideResourceRebindRollbackOpen(v->resources);
  message(v,e && e->message[0] ? e->message : "重命名失败，当前输入与原文件保留。"); okay=0;
 }
 if(active && active->open) xideEditorShow(a,active);
 xideReadingRestore(a,reading,0); xideWorkbenchLayout(a); xuiLayout(a->context); xideReadingRestore(a,reading,1); xrtValueRelease(reading);
 if(!okay) xideDialogSetOpen(v->window,1);
 xuiSetFocusWidget(a->context,xideRenameViewIsOpen(a) ? v->controls[v->pendingResources ? XIDE_RENAME_APPLY : XIDE_RENAME_NAME] : fromTree ? a->workspace : d->editor);
 /* A failed post-commit checkpoint cannot undo the successful physical rename.
    Publish current paths/partial resource bodies promptly and report any failure
    as a completed rename with a persistence warning, never as a retryable move. */
 if(moved) checkpointRenamed(a);
 xideActionsRefresh(a); xgeRenderRequest(); return okay;
}
static void clicked(xui_widget w,void* user)
{
 xide_app* a=user; xide_rename_view* v=a->renameView;
 if(w==v->controls[XIDE_RENAME_CANCEL]) { if(v->pendingResources) return; if(!v->mode) xideSessionCancelPreparedRename(a); xideDialogSetOpen(v->window,0); clear(v); xideActionsRefresh(a); }
 else { xide_model_error e={0}; if(!xideRenameViewApply(a,&e)) message(v,e.message); }
 xgeRenderRequest();
}
static int label(xide_app* a,xui_widget* out,const char* text,xui_rect_t rect)
{ xui_widget w=NULL; xui_label_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=text; d.iWrapMode=XUI_TEXT_WRAP_WORD; TRY(xuiLabelCreate(a->context,&w,&d)); TRY(xuiWidgetSetRect(w,rect)); TRY(xuiWidgetAddChild(a->renameView->content,w)); if(out) *out=w; return XUI_OK; }
static int input(xide_app* a,int id,const char* title,xui_rect_t rect,int readonly)
{ xui_widget* w=&a->renameView->controls[id]; xui_input_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; TRY(xuiInputCreate(a->context,w,&d)); TRY(xuiWidgetSetRect(*w,rect)); TRY(xuiWidgetSetAccessibleName(*w,title)); TRY(xuiInputSetReadonly(*w,readonly)); if(!readonly) { TRY(xuiEditSetEvent(*w,changed,a)); } return xuiWidgetAddChild(a->renameView->content,*w); }
static int button(xide_app* a,int id,const char* title,xui_rect_t rect)
{ xui_widget* w=&a->renameView->controls[id]; xui_button_desc_t d={0}; d.iSize=sizeof(d); d.pFont=a->uiFont; d.sText=title; TRY(xuiButtonCreate(a->context,w,&d)); TRY(xuiWidgetSetRect(*w,rect)); TRY(xuiButtonSetClick(*w,clicked,a)); return xuiWidgetAddChild(a->renameView->content,*w); }
static void windowClosed(xui_widget widget,void* user) { xide_app* a=user; clicked(a->renameView->controls[XIDE_RENAME_CANCEL],a); }
int xideRenameViewCreate(xide_app* a)
{
 xide_rename_view* v=calloc(1,sizeof(*v)); if(!v) return XUI_ERROR_OUT_OF_MEMORY; a->renameView=v;
 TRY(xideDialogCreate(a,&v->window,"重命名文件 / 迁移项目",680,356,14)); TRY(xideDialogSetCancel(v->window,windowClosed,a)); v->content=xideDialogGetContentWidget(v->window); TRY(xuiWidgetAddChild(a->root,v->window)); TRY(xuiWidgetSetLayoutType(v->content,XUI_LAYOUT_MANUAL));
 TRY(label(a,&v->title,"重命名当前文件",(xui_rect_t){0,0,680,28})); TRY(label(a,&v->originalLabel,"原文件",(xui_rect_t){0,44,100,28})); TRY(input(a,XIDE_RENAME_ORIGINAL,"原文件完整路径",(xui_rect_t){108,38,572,32},1));
 TRY(label(a,&v->nameLabel,"新文件名",(xui_rect_t){0,96,100,28})); TRY(input(a,XIDE_RENAME_NAME,"同一目录内的新文件名",(xui_rect_t){108,90,572,32},0));
 TRY(label(a,&v->targetLabel,"新路径",(xui_rect_t){0,148,100,28})); TRY(input(a,XIDE_RENAME_TARGET,"重命名后的完整路径",(xui_rect_t){108,142,572,32},1));
 TRY(label(a,&v->message,"",(xui_rect_t){0,198,680,96})); TRY(button(a,XIDE_RENAME_APPLY,"重命名",(xui_rect_t){460,318,104,32})); TRY(button(a,XIDE_RENAME_CANCEL,"取消",(xui_rect_t){580,318,100,32}));
 return xideUiLayoutCapture(&v->layout,v->content);
}
static int openFile(xide_app* a,const char* source,xide_document* d,int fromTree,xide_model_error* e)
{
 if(!source || xideActionsHasModal(a)) return fail(e,XIDE_MODEL_CONFLICT,"请先处理当前设置窗口。");
 xide_rename_view* v=a->renameView; xide_file_snapshot disk={0};
 if(fromTree && !xideFileSnapshotRead(source,&disk,e)) return 0;
 xvalue* state=baseline(a); char *path=copy(source),*language=d ? copy(d->language) : NULL,*documentPath=d ? copy(d->path) : NULL; unsigned char* raw=d ? malloc(d->baseline.rawLength+1) : NULL;
 if(!state || !path || (d && (!language || !documentPath || !raw))) { xrtValueRelease(state); free(path); free(language); free(documentPath); free(raw); return fail(e,XIDE_MODEL_MEMORY,"无法准备独立文件改名基线。"); }
 if(d && d->baseline.rawLength) { memcpy(raw,d->baseline.raw,d->baseline.rawLength); }
 xide_resource_rebind* resources=xideResourceRebindPrepare(&a->workspaceModel,&a->documents,source,e);
 if(!resources) { xrtValueRelease(state); free(path); free(language); free(documentPath); free(raw); return 0; }
 clear(v); v->model=state; v->original=path; v->documentPath=documentPath; v->language=language; v->raw=raw; v->rawLength=d ? d->baseline.rawLength : 0; v->document=d ? d->id : 0; v->version=d ? xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor)) : 0; v->dirty=d ? d->dirty : 0; v->languageOverride=d ? d->languageOverride : 0; v->fromTree=fromTree; v->disk=disk;
 v->resources=resources;
 xuiWidgetSetEnabled(v->controls[XIDE_RENAME_NAME],1); xuiWidgetSetEnabled(v->controls[XIDE_RENAME_CANCEL],1);
 xuiLabelSetText(v->title,fromTree ? "重命名所选文件" : "重命名当前文件"); xuiWindowSetTitle(v->window,fromTree ? "重命名所选文件" : "重命名当前文件");
 xuiLabelSetText(v->originalLabel,"原文件"); xuiLabelSetText(v->nameLabel,"新文件名"); xuiLabelSetText(v->targetLabel,"新路径");
 xuiWidgetSetAccessibleName(v->controls[XIDE_RENAME_NAME],"同一目录内的新文件名"); xuiButtonSetText(v->controls[XIDE_RENAME_APPLY],"重命名");
 xuiEditSetText(v->controls[XIDE_RENAME_ORIGINAL],source); xuiEditSetText(v->controls[XIDE_RENAME_NAME],xidePathFilename(source)); xideRenameViewRefresh(a); xideRenameViewLayout(a); xideDialogSetScroll(v->window,0,0); xideDialogSetOpen(v->window,1); xuiSetFocusWidget(a->context,v->controls[XIDE_RENAME_NAME]); xideActionsRefresh(a); xgeRenderRequest(); return 1;
}
int xideRenameViewOpen(xide_app* a)
{
 if(xideRenameViewIsOpen(a)) return 1;
 if(!xideRenameViewCanOpen(a)) return 0;
 return openFile(a,a->activeDocument->path,a->activeDocument,0,NULL);
}
int xideRenameViewOpenFile(xide_app* a,const char* path,xide_model_error* e)
{
 if(!xideRenameViewCanOpenFile(a,path)) return fail(e,XIDE_MODEL_INVALID,"请选择存在且可重命名的普通文件，并先处理项目属性。");
 return openFile(a,path,xideDocumentsFind(&a->documents,path),1,e);
}

int xideRenameViewOpenProject(xide_app* a,int mode,xide_model_error* e)
{
 if(!xideRenameViewCanMigrateProject(a) || (mode!=XIDE_MIGRATE_MANIFEST && mode!=XIDE_MIGRATE_DIRECTORY)) return fail(e,XIDE_MODEL_INVALID,"请选择可用项目，并先处理项目属性。");
 if(xideActionsHasModal(a)) return fail(e,XIDE_MODEL_CONFLICT,"请先完成当前设置窗口。");
 xide_project* p=xideWorkspaceFindProject(&a->workspaceModel,a->selectedProject);
 char* source=mode==XIDE_MIGRATE_DIRECTORY ? xidePathDirectory(p->manifest) : copy(p->manifest);
 xide_project_migration* baselinePlan=source ? xideProjectMigrationPrepare(&a->workspaceModel,p->id,source,mode,e) : NULL; free(source);
 if(!baselinePlan) return 0;
 xide_document_relocation* docs=xideDocumentsPrepareRelocation(&a->documents,NULL,NULL,e);
 char *original=copy(xideProjectMigrationSource(baselinePlan)),*id=copy(p->id),*session=a->projectSessionPath ? copy(a->projectSessionPath) : NULL;
 if(!docs || !original || !id || (a->projectSessionPath && !session)) { xideProjectMigrationDestroy(baselinePlan); xideDocumentsDestroyRelocation(docs); free(original); free(id); free(session); return fail(e,XIDE_MODEL_MEMORY,"无法准备独立迁移窗口基线。"); }
 xide_rename_view* v=a->renameView; clear(v); v->mode=mode+1; v->migrationBaseline=baselinePlan; v->documentBaseline=docs; v->original=original; v->projectId=id; v->projectSessionBefore=session;
 xuiWidgetSetEnabled(v->controls[XIDE_RENAME_NAME],1); xuiWidgetSetEnabled(v->controls[XIDE_RENAME_CANCEL],1);
 xuiLabelSetText(v->title,mode==XIDE_MIGRATE_DIRECTORY ? "迁移项目目录" : "迁移项目清单"); xuiWindowSetTitle(v->window,mode==XIDE_MIGRATE_DIRECTORY ? "迁移项目目录" : "迁移项目清单");
 xuiLabelSetText(v->originalLabel,"原位置"); xuiLabelSetText(v->nameLabel,"目标路径"); xuiLabelSetText(v->targetLabel,"完整路径");
 xuiWidgetSetAccessibleName(v->controls[XIDE_RENAME_NAME],"迁移目标完整路径"); xuiButtonSetText(v->controls[XIDE_RENAME_APPLY],"迁移");
 xuiEditSetText(v->controls[XIDE_RENAME_ORIGINAL],v->original); xuiEditSetText(v->controls[XIDE_RENAME_NAME],v->original); xideRenameViewRefresh(a); xideRenameViewLayout(a);
 xideDialogSetScroll(v->window,0,0); xideDialogSetOpen(v->window,1); xuiSetFocusWidget(a->context,v->controls[XIDE_RENAME_NAME]); xideActionsRefresh(a); xgeRenderRequest(); return 1;
}

int xideRenameViewIsOpen(xide_app* a) { return a && a->renameView && xideDialogIsOpen(a->renameView->window); }
int xideRenameViewHasPending(xide_app* a) { return a && a->renameView && a->renameView->pendingResources; }
void xideRenameViewLayout(xide_app* a)
{
 xide_rename_view* v=a->renameView; if(!v) return; int width=a->viewportWidth-48,height=a->viewportHeight-48; if(width<200) width=200; if(height<200) height=200;
 if(v->pendingResources && !xideDialogIsOpen(v->window)) xideDialogSetOpen(v->window,1);
 xideDialogSetMaxSize(v->window,(float)width,(float)height); float scale=xideUiFontScale(a);
 if(v->scale!=scale) { xideUiLayoutApply(a,&v->layout,1); xideDialogSetContentSize(v->window,xideUiHeight(a,680),xideUiHeight(a,356)); v->scale=scale; }
 
}
void xideRenameViewDestroy(xide_app* a) { if(a->renameView) { clear(a->renameView); free(a->renameView); a->renameView=NULL; } }
xui_widget xideRenameViewWindow(xide_app* a) { return a->renameView ? a->renameView->window : NULL; }
xui_widget xideRenameViewControl(xide_app* a,int i) { return a->renameView && i>=0 && i<XIDE_RENAME_CONTROL_COUNT ? a->renameView->controls[i] : NULL; }
const char* xideRenameViewMessage(xide_app* a) { return a->renameView ? a->renameView->messageText : ""; }
