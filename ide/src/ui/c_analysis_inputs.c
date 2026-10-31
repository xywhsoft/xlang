#include "app/features.h"
#if XIDE_ENABLE_C_LSP
#include "ui/c_analysis_inputs.h"
#include "ui/c_completion_view.h"
#include "ui/task_view.h"
#include "services/c_commands.h"
#include "services/execution_target.h"
#include "platform/path.h"
#include "platform/file_watch.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
static xstrview ccsv(const char* s) { return (xstrview){s,strlen(s)}; }
static xvalue* ccget(const xvalue* v,const char* k) { return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,ccsv(k)) : NULL; }
static int ccput(xvalue* v,const char* k,xvalue* child) { return child && xrtValueObjectSetNew(v,ccsv(k),child); }
static char* cccopy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static const char* ccstr(const xvalue* v) { xstrview s={0}; return v && xrtValueGetString(v,&s) && s.Size && s.Size<=32768 && !memchr(s.Data,0,s.Size) ? s.Data : NULL; }
int xideCInputNamed(xide_document* d) { return d && d->open && d->path && (xidePathHasExtension(d->path,".c") || xidePathHasExtension(d->path,".h")); }
static int ccxlang(xide_document* d) { return d && d->open && d->path && xidePathHasExtension(d->path,".xl"); }
static xvalue* ccmodel(xide_app* a)
{
 const char* compiler=xideTaskViewCompiler(a);
 xvalue *root=xrtValueObject(),*projects=xrtValueArray(); int ok=root && projects && ccput(root,"selected",xrtValueString(ccsv(a->selectedProject ? a->selectedProject : ""))) && ccput(root,"override",xrtValueString(ccsv(a->configurationId ? a->configurationId : ""))) && ccput(root,"compiler",xrtValueString(ccsv(compiler ? compiler : "")));
 for(size_t i=0;ok && i<a->workspaceModel.projectCount;++i) {
  xide_project* p=&a->workspaceModel.projects[i]; xvalue* item=xrtValueObject(); xvalue* source=p->available ? xideProjectExport(p,NULL) : xrtValueNull();
  const char* config=xideExecutionConfiguration(a,p);
  ok=item && source && ccput(item,"id",xrtValueString(ccsv(p->id))) && ccput(item,"manifest",xrtValueString(ccsv(p->manifest))) && ccput(item,"configuration",xrtValueString(ccsv(config ? config : ""))) && ccput(item,"project",source) && xrtValueArrayAppend(projects,item); xrtValueRelease(item);
 }
 if(ok) ok=xrtValueObjectSet(root,ccsv("projects"),projects);
 xrtValueRelease(projects); if(!ok) { xrtValueRelease(root); return NULL; } return root;
}
static int ccowns(const xide_project* p,const char* path)
{
 for(size_t i=0;i<p->cSources.count;++i) if(xidePathLexicalEqual(path,p->cSources.items[i])) return 1;
 for(size_t i=0;i<p->files.count;++i) if((!strcmp(p->files.items[i].kind,"c") || !strcmp(p->files.items[i].kind,"header")) && xidePathLexicalEqual(path,p->files.items[i].path)) return 1;
 return 0;
}
static int ccowner(xide_app* app,const char* path)
{
 int found=-1,ambiguous=0; for(size_t i=0;i<app->workspaceModel.projectCount;++i) if(ccowns(&app->workspaceModel.projects[i],path)) {
  if(app->selectedProject && !strcmp(app->selectedProject,app->workspaceModel.projects[i].id)) return (int)i;
  if(found!=-1) ambiguous=1;
  found=(int)i;
 } return ambiguous ? -2 : found;
}
/* An implicit navigation target keeps a project/source association, not file
   membership. A removed origin must not silently become a standalone file. */
static int cccontextOwner(xide_app* app,const xide_document* document,char** origin)
{
 *origin=NULL; int owner=ccowner(app,document->path);
 if(owner!=-1 || !document->cAnalysisContext) return owner;
 const xvalue* context=document->cAnalysisContext;
 if(!xideDocumentCContextValid(context)) return -3;
 const char *id=ccstr(ccget(context,"project")),*sourceId=ccstr(ccget(context,"sourceId"));
 for(size_t i=0;i<app->workspaceModel.projectCount;++i) {
  xide_project* p=&app->workspaceModel.projects[i]; if(strcmp(p->id,id)) continue;
  if(!p->available) return -3;
  if(sourceId) {
   for(size_t j=0;j<p->files.count;++j) if(!strcmp(p->files.items[j].id,sourceId)) {
    const xide_file* file=&p->files.items[j];
    if(strcmp(file->kind,"c") && strcmp(file->kind,"header")) return -3;
    *origin=cccopy(file->path); return *origin ? (int)i : -3;
   }
   return -3;
  }
  char* directory=xidePathDirectory(p->manifest);
  *origin=directory ? xidePathResolveLexical(directory,ccstr(ccget(context,"source"))) : NULL; free(directory);
  if(*origin && ccowns(p,*origin)) return (int)i;
  free(*origin); *origin=NULL; return -3;
 }
 return -3;
}
static int ccinputOwner(xide_c_inputs* b,const char* path,char** origin)
{
 for(size_t i=0;i<b->count;++i) if(xidePathLexicalEqual(b->items[i].path,path)) return cccontextOwner(b->app,b->items[i].document,origin);
 *origin=NULL; return ccowner(b->app,path);
}
int xideCInputsPrepareNavigation(xide_c_inputs* b,xide_document* source,const char* target,xvalue** context)
{
 if(!context) return 0;
 *context=NULL;
 if(!b || !source || !target || !xideCInputsCurrent(b) || !xideCInputsPath(b,source->path)) return 0;
 if(!xidePathHasExtension(target,".c") && !xidePathHasExtension(target,".h")) return 1;
 int targetOwner=ccowner(b->app,target); if(targetOwner!=-1) return targetOwner>=0;
 char* origin=NULL; int owner=cccontextOwner(b->app,source,&origin);
 if(owner<0) { free(origin); return owner==-1; }
 xide_project* p=&b->app->workspaceModel.projects[owner]; const char* path=origin ? origin : source->path;
 char* directory=xidePathDirectory(p->manifest); char* relative=directory ? xidePathRelative(directory,path) : NULL;
 xvalue* result=origin ? xrtValueDeepClone(source->cAnalysisContext) : xrtValueObject();
 int okay=relative && result && ccput(result,"project",xrtValueString(ccsv(p->id))) && ccput(result,"source",xrtValueString(ccsv(relative)));
 if(result) xrtValueObjectRemove(result,ccsv("sourceId"));
 for(size_t i=0;okay && i<p->files.count;++i) if(xidePathLexicalEqual(p->files.items[i].path,path)) { okay=ccput(result,"sourceId",xrtValueString(ccsv(p->files.items[i].id))); break; }
 okay=okay && xideDocumentCContextValid(result); free(origin); free(directory); free(relative);
 if(!okay) { xrtValueRelease(result); return 0; } *context=result; return 1;
}
static xvalue* ccstandalone(const char* path)
{
 char* directory=xidePathDirectory(path); xvalue *db=xrtValueArray(),*row=xrtValueObject(),*args=xrtValueArray();
 const char* fixed[]={"clang","-x","c","-std=c11","-D_CRT_SECURE_NO_WARNINGS","-target","x86_64-w64-windows-gnu","-I"}; int ok=directory && db && row && args;
 for(size_t i=0;ok && i<sizeof(fixed)/sizeof(*fixed);++i) ok=xrtValueArrayAppendNew(args,xrtValueString(ccsv(fixed[i])));
 if(ok) ok=xrtValueArrayAppendNew(args,xrtValueString(ccsv(directory))) && xrtValueArrayAppendNew(args,xrtValueString(ccsv("-c"))) && xrtValueArrayAppendNew(args,xrtValueString(ccsv(path))) && ccput(row,"directory",xrtValueString(ccsv(directory))) && ccput(row,"file",xrtValueString(ccsv(path))) && xrtValueObjectSet(row,ccsv("arguments"),args) && xrtValueArrayAppend(db,row);
 free(directory); xrtValueRelease(args); xrtValueRelease(row); if(!ok) { xrtValueRelease(db); return NULL; } return db;
}
static int ccmerge(xvalue* db,const xvalue* addition)
{
 for(size_t i=0;i<xrtValueCount(addition);++i) {
  xvalue* row=xrtValueArrayGet(addition,i); const char* file=ccstr(ccget(row,"file")); if(!file) return 0; int found=0;
  for(size_t j=0;j<xrtValueCount(db);++j) if(xidePathLexicalEqual(file,ccstr(ccget(xrtValueArrayGet(db,j),"file")))) { found=1; break; }
  if(!found && (xrtValueCount(db)>=2048 || !xrtValueArrayAppend(db,row))) return 0;
 } return 1;
}
static int ccaddPaths(xvalue* result,const char* directory,const xide_strings* strings)
{
 for(size_t i=0;i<strings->count;++i) { char* path=xidePathResolveLexical(directory,strings->items[i]); int okay=path && xrtValueArrayAppendNew(result,xrtValueString(ccsv(path))); free(path); if(!okay) return 0; } return 1;
}
/* Resolve the chosen context of shared files once, before copying any job.
   The selected owning project has priority, and ambiguous ownership rejects. */
static int ccbridges(xide_c_inputs* b,xide_model_error* e)
{
 int owners[128]; xvalue* databases[128]={0}; size_t count=0; int okay=1;
 for(size_t i=0;okay && i<xrtValueCount(b->commands);++i) {
  const char* file=ccstr(ccget(xrtValueArrayGet(b->commands,i),"file")); char* origin=NULL; int owner=ccinputOwner(b,file,&origin);
  if(owner<-1) { free(origin); okay=0; break; } if(owner<0) { free(origin); continue; }
  size_t slot=0; while(slot<count && owners[slot]!=owner) ++slot;
  if(slot==count) {
   if(count>=128) { free(origin); okay=0; break; }
   xide_project* project=&b->app->workspaceModel.projects[owner]; owners[count]=owner; databases[count++]=xideCCommandsCreate(project,xideExecutionConfiguration(b->app,project),NULL,e); if(!databases[slot]) { free(origin); okay=0; break; }
   if(b->extraCount>=128) { free(origin); okay=0; break; } b->extras[b->extraCount++]=project->manifest;
  }
  if(origin) okay=xideCCommandsAddContextFile(databases[slot],origin,file,e);
  free(origin); if(!okay) break;
  xvalue* row=NULL; for(size_t j=0;j<xrtValueCount(databases[slot]);++j) if(xidePathLexicalEqual(file,ccstr(ccget(xrtValueArrayGet(databases[slot],j),"file")))) row=xrtValueArrayGet(databases[slot],j);
  okay=row && xrtValueArraySet(b->commands,i,row);
 }
 for(size_t slot=0;okay && b->compiler && slot<count;++slot) {
  xide_project* p=&b->app->workspaceModel.projects[owners[slot]]; if(!p->entry || !xidePathHasExtension(p->entry,".xl")) continue;
  const char* config=xideExecutionConfiguration(b->app,p); xide_configuration* c=xideProjectFindConfiguration(p,config); char* directory=xidePathDirectory(p->manifest);
  xvalue *item=xrtValueObject(),*includes=xrtValueArray(),*sources=xrtValueArray(),*files=xrtValueArray(); char* entry=directory ? xidePathResolveLexical(directory,p->entry) : NULL;
  okay=c && directory && entry && item && includes && sources && files && ccaddPaths(includes,directory,&c->includePaths) && ccaddPaths(sources,directory,&p->cSources);
  for(size_t i=0;okay && i<xrtValueCount(b->commands);++i) { const char* file=ccstr(ccget(xrtValueArrayGet(b->commands,i),"file")); char* origin=NULL; if(ccinputOwner(b,file,&origin)==owners[slot]) okay=xrtValueArrayAppendNew(files,xrtValueString(ccsv(file))); free(origin); }
  if(okay) okay=ccput(item,"directory",xrtValueString(ccsv(directory))) && ccput(item,"entry",xrtValueString(ccsv(entry))) && ccput(item,"shared",xrtValueBool(!strcmp(p->kind,"dll") || !strcmp(p->kind,"shared"))) && xrtValueObjectSet(item,ccsv("includes"),includes) && xrtValueObjectSet(item,ccsv("cSources"),sources) && xrtValueObjectSet(item,ccsv("files"),files) && xrtValueArrayAppend(b->bridgeProjects,item);
  free(directory); free(entry); xrtValueRelease(item); xrtValueRelease(includes); xrtValueRelease(sources); xrtValueRelease(files);
 }
 for(size_t i=0;i<count;++i) xrtValueRelease(databases[i]);
 return okay;
}
static int cccapture(xide_c_input* item,xide_document* d,size_t* total)
{
 int n=xuiCodeDocumentGetLength(xuiCodeEditGetDocument(d->editor)); uint32_t version=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor));
 if(n<0 || (size_t)n>8u*1024u*1024u || (size_t)n>32u*1024u*1024u-*total || version>INT_MAX || !version || (d->externalState && d->externalState!=XIDE_WATCH_MISSING)) return 0;
 *item=(xide_c_input){d,d->id,version,d->externalState,cccopy(d->path),cccopy(xuiCodeEditGetText(d->editor)),cccopy(d->language),d->cAnalysisContext ? xrtValueDeepClone(d->cAnalysisContext) : NULL}; *total+=(size_t)n;
 return item->path && item->text && item->language && (!d->cAnalysisContext || item->context) && xideDocumentCContextValid(item->context);
}
void xideCInputsDestroy(xide_c_inputs* b)
{
 if(!b) return;
 for(size_t i=0;i<b->count;++i) { free(b->items[i].path); free(b->items[i].text); free(b->items[i].language); xrtValueRelease(b->items[i].context); }
 for(size_t i=0;i<b->xlangCount;++i) { free(b->xlang[i].path); free(b->xlang[i].text); free(b->xlang[i].language); xrtValueRelease(b->xlang[i].context); }
 free(b->server); free(b->program); free(b->directory); free(b->compiler); xrtValueRelease(b->model); xrtValueRelease(b->commands); xrtValueRelease(b->bridgeProjects); free(b);
}
xide_c_inputs* xideCInputsCapture(xide_app* app,xide_document* root,xide_model_error* e)
{
 const char* server=xideCCompletionViewServer(app); if(!xideCInputNamed(root) || !server) return NULL;
 xide_c_inputs* b=calloc(1,sizeof(*b)); if(!b) return NULL; b->app=app; const char* compiler=xideTaskViewCompiler(app);
 b->server=cccopy(server); b->program=xidePathResolveLexical(NULL,server); b->directory=xidePathDirectory(root->path); b->compiler=compiler ? xidePathResolveLexical(NULL,compiler) : NULL; b->model=ccmodel(app); b->commands=xrtValueArray(); b->bridgeProjects=xrtValueArray();
 int okay=b->server && b->program && b->directory && (!compiler || b->compiler) && b->model && b->commands && b->bridgeProjects; size_t total=0;
 for(int first=1;okay && first>=0;--first) for(size_t i=0;okay && i<app->documents.count;++i) {
  xide_document* d=app->documents.items[i]; if(!xideCInputNamed(d) || (d==root)!=first) continue;
  if(b->count>=64) { okay=0; break; } xide_c_input* item=&b->items[b->count++]; okay=cccapture(item,d,&total); if(!okay) break;
 }
 int owners[64]; xvalue* databases[64]={0}; size_t ownerCount=0;
 for(size_t i=0;okay && i<b->count;++i) {
  xide_c_input* item=&b->items[i]; char* origin=NULL; int owner=cccontextOwner(app,item->document,&origin);
  if(owner>=0) {
   size_t slot=0; while(slot<ownerCount && owners[slot]!=owner) ++slot;
   if(slot==ownerCount) { owners[ownerCount]=owner; databases[ownerCount++]=xideCCommandsCreate(&app->workspaceModel.projects[owner],xideExecutionConfiguration(app,&app->workspaceModel.projects[owner]),NULL,e); }
   okay=databases[slot] && (!origin || xideCCommandsAddContextFile(databases[slot],origin,item->path,e));
  } else if(owner==-1) { xvalue* addition=ccstandalone(item->path); okay=addition && ccmerge(b->commands,addition); xrtValueRelease(addition); }
  else okay=0;
  free(origin);
 }
 for(size_t i=0;i<ownerCount;++i) { if(okay) okay=ccmerge(b->commands,databases[i]); xrtValueRelease(databases[i]); }
 if(okay) okay=ccbridges(b,e);
 total=0;
 for(size_t i=0;okay && xrtValueCount(b->bridgeProjects) && i<app->documents.count;++i) {
  xide_document* d=app->documents.items[i]; if(!ccxlang(d)) continue; if(b->xlangCount>=64) { okay=0; break; }
  okay=cccapture(&b->xlang[b->xlangCount++],d,&total);
 }
 if(!okay || !b->count) { if(e && !*e->message) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"C/xlang inputs are ambiguous, conflicting or exceed capacity."); } xideCInputsDestroy(b); return NULL; } return b;
}
static int ccitemsCurrent(xide_c_input* items,size_t count,int xlang)
{
 for(size_t i=0;i<count;++i) { xide_c_input* item=&items[i]; xide_document* d=item->document;
  if(!(xlang ? ccxlang(d) : xideCInputNamed(d)) || d->id!=item->id || d->externalState!=item->external || strcmp(d->path,item->path) || strcmp(d->language,item->language) || item->version!=xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(d->editor))) return 0;
  if((item->context || d->cAnalysisContext) && (!item->context || !d->cAnalysisContext || !xrtValueEqual(item->context,d->cAnalysisContext))) return 0;
 } return 1;
}
int xideCInputsCurrent(xide_c_inputs* b)
{
 if(!b) return 0;
 size_t count=0,xlang=0;
 for(size_t i=0;i<b->app->documents.count;++i) { xide_document* d=b->app->documents.items[i]; if(xideCInputNamed(d)) ++count; if(xrtValueCount(b->bridgeProjects) && ccxlang(d)) ++xlang; }
 if(count!=b->count || !count || xlang!=b->xlangCount || !ccitemsCurrent(b->items,b->count,0) || !ccitemsCurrent(b->xlang,b->xlangCount,1)) return 0;
 xvalue* model=ccmodel(b->app); const char* server=xideCCompletionViewServer(b->app); int okay=model && xrtValueEqual(model,b->model) && server && !strcmp(server,b->server); xrtValueRelease(model); return okay;
}
xide_c_input* xideCInputsPath(xide_c_inputs* b,const char* path)
{ if(b && path) for(size_t i=0;i<b->count;++i) if(xidePathLexicalEqual(b->items[i].path,path)) return &b->items[i]; return NULL; }
xide_c_input* xideCInputsXlangPath(xide_c_inputs* b,const char* path)
{ if(b && path) for(size_t i=0;i<b->xlangCount;++i) if(xidePathLexicalEqual(b->xlang[i].path,path)) return &b->xlang[i]; return NULL; }
xide_c_job_spec xideCInputsSpec(xide_c_inputs* b,xide_document* root,int offset,void (*wake)(void*))
{
 static _Thread_local xide_c_document docs[64]; static _Thread_local xide_language_snapshot xlang[64];
 for(size_t i=0;i<b->count;++i) docs[i]=(xide_c_document){b->items[i].id,b->items[i].path,b->items[i].text,(int)b->items[i].version};
 for(size_t i=0;i<b->xlangCount;++i) xlang[i]=(xide_language_snapshot){b->xlang[i].path,b->xlang[i].text,(int)b->xlang[i].version};
 b->bridge=(xide_c_bridge_spec){b->compiler,b->bridgeProjects,xlang,b->xlangCount};
 return (xide_c_job_spec){.server=b->program,.directory=b->directory,.commands=b->commands,.documents=docs,.count=b->count,.documentId=root->id,.offset=offset,.extraFiles=b->extras,.extraCount=b->extraCount,.wake=wake,.user=b->app,.bridge=xrtValueCount(b->bridgeProjects) ? &b->bridge : NULL};
}

#endif /* XIDE_ENABLE_C_LSP */
