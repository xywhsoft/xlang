#include "services/project_migration.h"
#include "platform/path.h"
#include "platform/file_rename.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef struct path_patch { char** slot; char *before,*value; } path_patch;
struct xide_project_migration {
 xide_workspace* workspace; xide_project* selected; char *source,*target; int mode,applied,workspaceDirty,workspaceMoves;
 path_patch* paths; size_t count,capacity; xide_project** projects; size_t projectCount;
 xvalue* baseline; xide_file_snapshot file; xide_directory_identity directory;
};
static char* copy(const char* s) { if(!s) s=""; size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int fail(xide_model_error* e,int code,const char* s) { if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static xstrview sv(const char* s) { return (xstrview){s,strlen(s)}; }
static int put(xvalue* v,const char* k,const char* s) { return xrtValueObjectSetNew(v,sv(k),xrtValueString(sv(s ? s : ""))); }
static int pointer(xvalue* v,const char* k,const void* p) { char text[40]; snprintf(text,sizeof(text),"%p",p); return put(v,k,text); }
static int within(const char* path,const char* root) { size_t n=strlen(root),length=strlen(path); return length>=n && !memcmp(path,root,n) && (length==n || path[n]=='/'); }
/* Canonical absolute paths and complete directory components only. Outside
   hardlink spellings remain outside when the containing directory is moved. */
char* xideProjectMigrationMapPath(const xide_project_migration* p,const char* path)
{
 if(!p || !path) return NULL;
 if(p->mode==XIDE_MIGRATE_MANIFEST) return copy(xidePathEqual(path,p->source) ? p->target : path);
 if(!within(path,p->source)) return copy(path);
 size_t a=strlen(p->target),b=strlen(path+strlen(p->source)); if(a>SIZE_MAX-b-1) return NULL;
 char* result=malloc(a+b+1); if(result) { memcpy(result,p->target,a); memcpy(result+a,path+strlen(p->source),b+1); } return result;
}
static xvalue* state(const xide_workspace* w)
{
 xvalue *o=xrtValueObject(),*projects=xrtValueArray(),*files=xrtValueArray();
 int okay=o && projects && files && put(o,"manifest",w->manifest) && put(o,"name",w->name) && put(o,"startup",w->startupProject) && pointer(o,"projectArray",w->projects) && pointer(o,"fileArray",w->files.items) && xrtValueObjectSetNew(o,sv("dirty"),xrtValueBool(w->dirty!=0)) && (!w->source || xrtValueObjectSet(o,sv("source"),w->source));
 for(size_t i=0;okay && i<w->files.count;++i) { const xide_file* f=&w->files.items[i]; xvalue* item=xrtValueObject(); okay=item && put(item,"id",f->id) && put(item,"path",f->path) && put(item,"kind",f->kind) && xrtValueObjectSet(item,sv("source"),f->source); if(okay) okay=xrtValueArrayAppendNew(files,item); else xrtValueRelease(item); }
 for(size_t i=0;okay && i<w->projectCount;++i) {
  const xide_project* p=&w->projects[i]; xvalue *item=xrtValueObject(),*arrays=xrtValueArray(),*data=p->available ? xideProjectExport(p,NULL) : xrtValueObject();
  okay=item && arrays && data && put(item,"id",p->id) && put(item,"manifest",p->manifest) && put(item,"issue",p->issue) && pointer(item,"files",p->files.items) && pointer(item,"c",p->cSources.items) && pointer(item,"resources",p->resources.items) && pointer(item,"configs",p->configurations) && xrtValueObjectSetNew(item,sv("available"),xrtValueBool(p->available!=0)) && xrtValueObjectSetNew(item,sv("dirty"),xrtValueBool(p->dirty!=0)) && xrtValueObjectSet(item,sv("data"),data) && (!p->source || xrtValueObjectSet(item,sv("source"),p->source)) && (!p->referenceSource || xrtValueObjectSet(item,sv("reference"),p->referenceSource));
  for(size_t j=0;okay && j<p->configurationCount;++j) { xvalue* a=xrtValueObject(); okay=a && pointer(a,"include",p->configurations[j].includePaths.items) && pointer(a,"library",p->configurations[j].libraryPaths.items) && pointer(a,"cSystem",p->configurations[j].cAnalysis.systemIncludes.items) && pointer(a,"cForced",p->configurations[j].cAnalysis.forcedIncludes.items); if(okay) okay=xrtValueArrayAppendNew(arrays,a); else xrtValueRelease(a); }
  if(okay) okay=xrtValueObjectSet(item,sv("arrays"),arrays);
  xrtValueRelease(arrays); xrtValueRelease(data); if(okay) okay=xrtValueArrayAppendNew(projects,item); else xrtValueRelease(item);
 }
 if(okay) okay=xrtValueObjectSet(o,sv("projects"),projects) && xrtValueObjectSet(o,sv("files"),files);
 xrtValueRelease(projects); xrtValueRelease(files); xvalue* independent=okay ? xrtValueDeepClone(o) : NULL; xrtValueRelease(o); return independent;
}
static int patch(xide_project_migration* p,char** slot,xide_model_error* e)
{
 if(!*slot || !**slot) return 1;
 char* value=xideProjectMigrationMapPath(p,*slot); if(!value) return fail(e,XIDE_MODEL_MEMORY,"无法准备迁移路径");
 if(!strcmp(value,*slot)) { free(value); return 1; }
 if(p->count==p->capacity) { size_t cap=p->capacity ? p->capacity*2 : 32; if(cap<p->capacity || cap>SIZE_MAX/sizeof(path_patch)) { free(value); return fail(e,XIDE_MODEL_MEMORY,"迁移路径过多"); } path_patch* next=realloc(p->paths,cap*sizeof(*next)); if(!next) { free(value); return fail(e,XIDE_MODEL_MEMORY,"无法准备迁移补丁"); } p->paths=next; p->capacity=cap; }
 char* before=copy(*slot); if(!before) { free(value); return fail(e,XIDE_MODEL_MEMORY,"无法保存迁移前路径"); }
 p->paths[p->count++]=(path_patch){slot,before,value}; return 1;
}
static const char* after(const xide_project_migration* p,char* const* slot)
{ for(size_t i=0;i<p->count;++i) if(p->paths[i].slot==slot) return p->paths[i].value; return *slot; }
static int kindValid(const char* kind,const char* path,xide_model_error* e)
{
 const char* ext=!strcmp(kind,"xlang") ? ".xl" : !strcmp(kind,"c") ? ".c" : !strcmp(kind,"header") ? ".h" : NULL;
 if(ext && !xidePathHasExtension(path,ext)) return fail(e,XIDE_MODEL_INVALID,"迁移不能改变工程文件种类的扩展名");
 if(!strcmp(kind,"resource-config") && !xidePathHasExtension(path,".json") && !xidePathHasExtension(path,".xlon")) return fail(e,XIDE_MODEL_INVALID,"迁移不能改变编译资源种类");
 return 1;
}
static int files(xide_project_migration* p,xide_files* files,xide_model_error* e)
{
 for(size_t i=0;i<files->count;++i) if(!patch(p,&files->items[i].path,e) || !kindValid(files->items[i].kind,after(p,&files->items[i].path),e)) return 0;
 for(size_t i=0;i<files->count;++i) for(size_t j=0;j<i;++j) if(xidePathEqual(after(p,&files->items[i].path),after(p,&files->items[j].path))) return fail(e,XIDE_MODEL_CONFLICT,"迁移目标与已有文件引用重复");
 return 1;
}
static int strings(xide_project_migration* p,xide_strings* list,const char* kind,xide_model_error* e)
{
 for(size_t i=0;i<list->count;++i) if(!patch(p,&list->items[i],e) || (kind && !kindValid(kind,after(p,&list->items[i]),e))) return 0;
 for(size_t i=0;i<list->count;++i) for(size_t j=0;j<i;++j) if(xidePathEqual(after(p,&list->items[i]),after(p,&list->items[j]))) return fail(e,XIDE_MODEL_CONFLICT,"迁移目标与已有构建路径重复");
 return 1;
}
static int jsonUnchanged(const char* path,const xvalue* baseline,xide_model_error* e)
{
 size_t length=0; unsigned char* bytes=xrtFileReadAllLimit(path,16u*1024u*1024u,&length); size_t start=bytes && length>=3 && !memcmp(bytes,"\xef\xbb\xbf",3) ? 3 : 0;
 xvalue* current=bytes ? xrtJsonParse((xstrview){(const char*)bytes+start,length-start}) : NULL; xrtFree(bytes); int same=current && baseline && xrtValueEqual(current,baseline); xrtValueRelease(current);
 return same ? 1 : fail(e,XIDE_MODEL_CONFLICT,"受影响清单在磁盘已改变，请先处理磁盘变化");
}
static int manifestUnchanged(const xide_project* p,xide_model_error* e) { return jsonUnchanged(p->manifest,p->source,e); }
void xideProjectMigrationDestroy(xide_project_migration* p)
{ if(p) { for(size_t i=0;i<p->count;++i) { free(p->paths[i].before); free(p->paths[i].value); } free(p->paths); free(p->projects); free(p->source); free(p->target); xrtValueRelease(p->baseline); free(p); } }
xide_project_migration* xideProjectMigrationPrepare(xide_workspace* w,const char* id,const char* destination,int mode,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); }
 xide_project* selected=xideWorkspaceFindProject(w,id); if(!selected || !selected->available || !destination || (mode!=XIDE_MIGRATE_DIRECTORY && mode!=XIDE_MIGRATE_MANIFEST)) { fail(e,XIDE_MODEL_INVALID,"请选择可用项目和迁移目标"); return NULL; }
 xide_project_migration* p=calloc(1,sizeof(*p)); if(!p) { fail(e,XIDE_MODEL_MEMORY,"无法准备迁移事务"); return NULL; } p->workspace=w; p->selected=selected; p->mode=mode;
 char* source=mode==XIDE_MIGRATE_DIRECTORY ? xidePathDirectory(selected->manifest) : copy(selected->manifest);
 char *lexical=xidePathResolveLexical(NULL,destination),*parent=lexical ? xidePathDirectory(lexical) : NULL,*canonicalParent=parent ? xidePathResolve(NULL,parent) : NULL;
 p->target=canonicalParent && xidePathIsDirectory(canonicalParent) ? xidePathResolveLexical(canonicalParent,xidePathFilename(lexical)) : NULL;
 free(lexical); free(parent); free(canonicalParent);
 int okay=source && p->target && xideFileRenameTargetValid(p->target,e);
 if(okay && mode==XIDE_MIGRATE_DIRECTORY) okay=strlen(source)>3 && xideDirectoryIdentityRead(source,&p->directory,e);
 if(okay && mode==XIDE_MIGRATE_MANIFEST) okay=xidePathHasExtension(p->target,".xlp") && xideFileSnapshotRead(source,&p->file,e);
 p->source=source ? xidePathResolve(NULL,source) : NULL; free(source);
 if(okay && (!p->source || (mode==XIDE_MIGRATE_DIRECTORY && strcmp(p->source,p->target) && within(p->target,p->source)))) okay=fail(e,XIDE_MODEL_INVALID,"不能将项目目录迁移到其内部");
 if(okay && !strcmp(p->source,p->target)) { p->baseline=state(w); if(p->baseline) return p; okay=0; }
 if(okay) { p->projects=calloc(w->projectCount+1,sizeof(*p->projects)); okay=p->projects!=NULL; }
 size_t before=p->count;
 if(okay && mode==XIDE_MIGRATE_DIRECTORY) okay=patch(p,&w->manifest,e);
 p->workspaceMoves=p->count!=before; p->workspaceDirty=p->workspaceMoves;
 if(okay) { okay=files(p,&w->files,e); } p->workspaceDirty|=p->count!=before;
 for(size_t i=0;okay && i<w->projectCount;++i) {
  xide_project* project=&w->projects[i]; before=p->count; okay=patch(p,&project->manifest,e); p->workspaceDirty|=p->count!=before;
  if(project->available && okay) {
   okay=patch(p,&project->entry,e) && kindValid("xlang",after(p,&project->entry),e) && files(p,&project->files,e) && strings(p,&project->cSources,"c",e) && strings(p,&project->resources,"resource-config",e);
   for(size_t j=0;okay && j<project->configurationCount;++j) { xide_configuration* c=&project->configurations[j]; okay=patch(p,&c->output,e) && patch(p,&c->workingDirectory,e) && patch(p,&c->test.program,e) && patch(p,&c->test.workingDirectory,e) && strings(p,&c->includePaths,NULL,e) && strings(p,&c->libraryPaths,NULL,e) && strings(p,&c->cAnalysis.systemIncludes,NULL,e) && strings(p,&c->cAnalysis.forcedIncludes,NULL,e); }
   if(okay && p->count!=before) { p->projects[p->projectCount++]=project; okay=manifestUnchanged(project,e); }
  }
 }
 for(size_t i=0;okay && i<w->projectCount;++i) {
  if(w->manifest && xidePathEqual(after(p,&w->manifest),after(p,&w->projects[i].manifest))) okay=fail(e,XIDE_MODEL_CONFLICT,"迁移目标与工作空间清单冲突");
  for(size_t j=0;okay && j<i;++j) if(xidePathEqual(after(p,&w->projects[i].manifest),after(p,&w->projects[j].manifest))) okay=fail(e,XIDE_MODEL_CONFLICT,"迁移目标与已有项目清单引用冲突");
 }
 if(okay && p->workspaceDirty && w->manifest) okay=jsonUnchanged(w->manifest,w->source,e);
 if(okay) { p->baseline=state(w); okay=p->baseline!=NULL; }
 if(!okay) { if(e && !e->code) fail(e,XIDE_MODEL_INVALID,"无效迁移目标或无法准备完整事务"); xideProjectMigrationDestroy(p); return NULL; } return p;
}
static void applyModel(xide_project_migration* p)
{
 for(size_t i=0;i<p->count;++i) { free(*p->paths[i].slot); *p->paths[i].slot=p->paths[i].value; p->paths[i].value=NULL; }
 for(size_t i=0;i<p->projectCount;++i) p->projects[i]->dirty=1;
 if(p->workspaceDirty) { p->workspace->dirty=1; } p->applied=1;
}
int xideProjectMigrationApplyPreview(xide_project_migration* p,xide_model_error* e)
{
 if(!p || p->applied) return fail(e,XIDE_MODEL_INVALID,"迁移预览已结束");
 xvalue* now=state(p->workspace); int same=now && xrtValueEqual(now,p->baseline); xrtValueRelease(now);
 if(!same) return fail(e,XIDE_MODEL_CONFLICT,"迁移预览模型已改变");
 applyModel(p); return 1;
}
int xideProjectMigrationCommit(xide_project_migration* p,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); } if(!p || p->applied) return fail(e,XIDE_MODEL_INVALID,"迁移事务已结束");
 xvalue* current=state(p->workspace); int same=current && xrtValueEqual(current,p->baseline); xrtValueRelease(current); if(!same) return fail(e,XIDE_MODEL_CONFLICT,"工程模型或集合已改变，请重新准备迁移");
 if(!strcmp(p->source,p->target)) { p->applied=1; return 1; }
 if(p->workspaceDirty && p->workspace->manifest && !jsonUnchanged(p->workspace->manifest,p->workspace->source,e)) return 0;
 for(size_t i=0;i<p->projectCount;++i) if(!manifestUnchanged(p->projects[i],e)) return 0;
 int okay=p->mode==XIDE_MIGRATE_DIRECTORY ? xideDirectoryRenameChecked(p->source,p->target,&p->directory,e) : xideFileRenameChecked(p->source,p->target,NULL,&p->file,e);
 if(!okay) return 0;
 applyModel(p); return 1;
}
const char* xideProjectMigrationSource(const xide_project_migration* p) { return p ? p->source : NULL; }
const char* xideProjectMigrationTarget(const xide_project_migration* p) { return p ? p->target : NULL; }
int xideProjectMigrationMode(const xide_project_migration* p) { return p ? p->mode : -1; }
size_t xideProjectMigrationPathCount(const xide_project_migration* p) { return p ? p->count : 0; }
const char* xideProjectMigrationPathBefore(const xide_project_migration* p,size_t i) { return p && i<p->count ? p->paths[i].before : NULL; }
const char* xideProjectMigrationPathAfter(const xide_project_migration* p,size_t i) { return p && i<p->count ? (p->applied ? *p->paths[i].slot : p->paths[i].value) : NULL; }
size_t xideProjectMigrationProjectCount(const xide_project_migration* p) { return p ? p->projectCount : 0; }
xide_project* xideProjectMigrationProjectAt(const xide_project_migration* p,size_t i) { return p && i<p->projectCount ? p->projects[i] : NULL; }
int xideProjectMigrationWorkspaceMoves(const xide_project_migration* p) { return p && p->workspaceMoves; }


xide_project_migration* xideProjectMigrationRetarget(const xide_project_migration* old,const char* target,xide_model_error* e)
{
 if(!old || old->applied) { fail(e,XIDE_MODEL_INVALID,"迁移窗口基线已结束"); return NULL; }
 xvalue* now=state(old->workspace); int same=now && xrtValueEqual(now,old->baseline); xrtValueRelease(now);
 if(!same) { fail(e,XIDE_MODEL_CONFLICT,"工程模型或集合已改变，请重新打开迁移窗口"); return NULL; }
 xide_project_migration* p=xideProjectMigrationPrepare(old->workspace,old->selected->id,target,old->mode,e); if(!p) return NULL;
 same=!strcmp(p->source,old->source) && xrtValueEqual(p->baseline,old->baseline);
 if(same) same=p->mode==XIDE_MIGRATE_DIRECTORY ? (p->directory.index==old->directory.index && p->directory.volume==old->directory.volume) : xideFileSnapshotEqual(&p->file,&old->file);
 if(!same) { xideProjectMigrationDestroy(p); fail(e,XIDE_MODEL_CONFLICT,"源项目或磁盘身份已改变，请重新打开迁移窗口"); return NULL; }
 return p;
}
