#include "services/file_rebind.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct path_patch { char** slot; char* value; } path_patch;
struct xide_file_rebind {
 path_patch* patches; size_t count,capacity;
 xide_project** projects; size_t projectCount;
 xide_workspace* workspace; int workspaceFiles,applied;
};
static int fail(xide_model_error* e,int code,const char* text)
{ if(e) { e->code=code; snprintf(e->message,sizeof(e->message),"%s",text); } return 0; }
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
void xideFileRebindDestroy(xide_file_rebind* p)
{ if(p) { for(size_t i=0;i<p->count;++i) free(p->patches[i].value); free(p->patches); free(p->projects); free(p); } }
static int patch(xide_file_rebind* p,char** slot,const char* target,xide_model_error* e)
{
 if(p->count==p->capacity) { size_t cap=p->capacity ? p->capacity*2 : 16; if(cap<p->capacity || cap>SIZE_MAX/sizeof(path_patch)) return fail(e,XIDE_MODEL_MEMORY,"引用事务过大。"); path_patch* next=realloc(p->patches,cap*sizeof(*next)); if(!next) return fail(e,XIDE_MODEL_MEMORY,"无法准备文件引用事务。"); p->patches=next; p->capacity=cap; }
 char* value=copy(target); if(!value) return fail(e,XIDE_MODEL_MEMORY,"无法准备引用路径。"); p->patches[p->count++]=(path_patch){slot,value}; return 1;
}
static int extension(const char* kind,const char* path,xide_model_error* e)
{
 const char* ext=!strcmp(kind,"xlang") ? ".xl" : !strcmp(kind,"c") ? ".c" : !strcmp(kind,"header") ? ".h" : NULL;
 if(ext && !xidePathHasExtension(path,ext)) return fail(e,XIDE_MODEL_INVALID,"新路径须保持工程文件种类的扩展名；独立文档可另存为其他语法。");
 if(!strcmp(kind,"resource-config") && !xidePathHasExtension(path,".json") && !xidePathHasExtension(path,".xlon")) return fail(e,XIDE_MODEL_INVALID,"编译资源配置须为 .json 或 .xlon。");
 return 1;
}
static int files(xide_file_rebind* p,xide_files* list,const char* old,const char* target,xide_model_error* e)
{
 size_t changed=0; int occupied=0;
 for(size_t i=0;i<list->count;++i) { xide_file* f=&list->items[i]; if(xidePathEqual(f->path,old)) { if(!extension(f->kind,target,e) || !patch(p,&f->path,target,e)) return 0; ++changed; } else if(xidePathEqual(f->path,target)) occupied=1; }
 if(changed && (occupied || changed>1)) return fail(e,XIDE_MODEL_CONFLICT,"新路径与已有工程引用重复，或多个引用已指向同一物理文件。请先处理引用冲突。");
 return 1;
}
static int strings(xide_file_rebind* p,xide_strings* list,const char* old,const char* target,const char* kind,xide_model_error* e)
{
 size_t changed=0; int occupied=0;
 for(size_t i=0;i<list->count;++i) { if(xidePathEqual(list->items[i],old)) { if(!extension(kind,target,e) || !patch(p,&list->items[i],target,e)) return 0; ++changed; } else if(xidePathEqual(list->items[i],target)) occupied=1; }
 if(changed && (occupied || changed>1)) return fail(e,XIDE_MODEL_CONFLICT,"新路径与已有构建输入重复。请先处理引用冲突。");
 return 1;
}
int xideFileRebindPrepare(xide_workspace* w,const char* old,const char* target,xide_file_rebind** out,xide_model_error* e)
{
 if(e) { memset(e,0,sizeof(*e)); }
 if(!out || !target) { return fail(e,XIDE_MODEL_INVALID,"无效的文件路径事务。"); } *out=NULL;
 if(!w || !w->source || !old || !strcmp(old,target)) return 1;
 if((w->manifest && (xidePathEqual(old,w->manifest) || xidePathEqual(target,w->manifest)))) return fail(e,XIDE_MODEL_CONFLICT,"已绑定工作空间清单请使用工作空间另存为；源文件另存为不能覆盖清单。");
 for(size_t i=0;i<w->projectCount;++i) if(xidePathEqual(old,w->projects[i].manifest) || xidePathEqual(target,w->projects[i].manifest)) return fail(e,XIDE_MODEL_CONFLICT,"已绑定项目清单需要独立项目迁移操作；源文件另存为不能改绑或覆盖清单。");
 xide_file_rebind* p=calloc(1,sizeof(*p)); if(!p) return fail(e,XIDE_MODEL_MEMORY,"无法准备引用事务。"); p->workspace=w;
 p->projects=w->projectCount ? calloc(w->projectCount,sizeof(*p->projects)) : NULL;
 int okay=!w->projectCount || p->projects;
 for(size_t i=0;okay && i<w->projectCount;++i) {
  xide_project* project=&w->projects[i]; size_t before=p->count;
  if(!project->available) continue;
  if(xidePathEqual(project->entry,old)) okay=extension("xlang",target,e) && patch(p,&project->entry,target,e);
  if(okay) okay=files(p,&project->files,old,target,e) && strings(p,&project->cSources,old,target,"c",e) && strings(p,&project->resources,old,target,"resource-config",e);
  for(size_t j=0;okay && j<project->configurationCount;++j) {
   xide_configuration* c=&project->configurations[j]; okay=strings(p,&c->cAnalysis.forcedIncludes,old,target,"text",e);
   if(okay && c->test.program && *c->test.program && xidePathEqual(c->test.program,old)) okay=patch(p,&c->test.program,target,e);
  }
  if(p->count!=before) p->projects[p->projectCount++]=project;
 }
 size_t before=p->count; if(okay) okay=files(p,&w->files,old,target,e); p->workspaceFiles=p->count!=before;
 if(!okay) { xideFileRebindDestroy(p); if(e && !e->code) fail(e,XIDE_MODEL_MEMORY,"无法准备工程路径事务。"); return 0; }
 if(!p->count) { xideFileRebindDestroy(p); return 1; } *out=p; return 1;
}
void xideFileRebindApply(xide_file_rebind* p)
{
 if(!p || p->applied) { return; } p->applied=1;
 for(size_t i=0;i<p->count;++i) { free(*p->patches[i].slot); *p->patches[i].slot=p->patches[i].value; p->patches[i].value=NULL; }
 for(size_t i=0;i<p->projectCount;++i) p->projects[i]->dirty=1;
 if(p->workspaceFiles) p->workspace->dirty=1;
}
size_t xideFileRebindProjectCount(const xide_file_rebind* p) { return p ? p->projectCount : 0; }
xide_project* xideFileRebindProjectAt(const xide_file_rebind* p,size_t index) { return p && index<p->projectCount ? p->projects[index] : NULL; }
