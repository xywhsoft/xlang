#include "services/edit_request.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"edit-request-scope:%d:%s\n",__LINE__,#v);return 1;}}while(0)
#ifdef XLS_COMPILER_XRT
static int probing;static size_t allocationCalls,allocationFailure;
void* __real_xrtMalloc(size_t);void* __real_xrtCalloc(size_t,size_t);
void* __wrap_xrtMalloc(size_t n){if(probing&&++allocationCalls==allocationFailure)return NULL;return __real_xrtMalloc(n);}
void* __wrap_xrtCalloc(size_t n,size_t z){if(probing&&++allocationCalls==allocationFailure)return NULL;return __real_xrtCalloc(n,z);}
#endif
typedef struct roster {xide_edit_request_model* models;size_t count;} roster;
static int modelAt(void* raw,size_t i,xide_edit_request_model* out)
{roster* r=raw;if(i>=r->count)return 0;*out=r->models[i];return 1;}
static int put(xvalue* o,const char* k,xvalue* v){return v&&xrtValueObjectSetNew(o,xrtStrView(k),v);}
static xvalue* catalog(int project)
{
 xvalue* out=xrtValueObject(),*list=xrtValueArray();int okay=out&&list;
 if(project){const char* members[]={"F:/scope/shared.xl"};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project="F:/scope/project/A.xlp",.configuration="dev",.entry="F:/scope/project/main.xl",.directory="F:/scope/project",.analysisProfile=XIDE_ANALYSIS_PROFILE,.targetKind="exe",.packagePath=""};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,1};xls_context* c=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xvalue* wire=c?xideAnalysisContextWrite(c):NULL;xlsContextRelease(c);okay=okay&&wire&&xrtValueArrayAppendNew(list,wire);}
 if(okay)okay=put(out,"schema",xrtValueInt(1))&&put(out,"selectedProject",xrtValueString(xrtStrView(project?"F:/scope/project/A.xlp":"")))&&put(out,"packagePath",xrtValueString(xrtStrView("")));
 if(okay){okay=put(out,"projects",list);list=NULL;}xrtValueRelease(list);if(!okay){xrtValueRelease(out);out=NULL;}return out;
}
typedef struct cancel {size_t calls,at;xide_edit_request* invalidate;} cancel;
static int cancelled(void* raw){cancel* c=raw;++c->calls;if(c->at==c->calls){if(c->invalidate)xideEditRequestInvalidate(c->invalidate,XIDE_EDIT_REQUEST_CONFIGURATION);return 1;}return 0;}
static xide_edit_request* request(xls_sync* sync,xvalue* cat,const xide_files* files,roster* models)
{return xideEditRequestCreate(1,7,sync,cat,files,models?models->count:0,modelAt,models,NULL);}
static int owners(const xide_rename_scope* s,const char* path,uint64_t expected)
{uint64_t bits=UINT64_MAX;return xideRenameScopeOwners(s,path,&bits)&&bits==expected;}
typedef struct reader {xide_rename_scope* scope;int okay;} reader;
static int readScope(void* raw)
{reader* r=raw;r->okay=1;for(int i=0;i<1000;++i)if(xideRenameScopeCount(r->scope)!=4||!owners(r->scope,"F:/scope/one.xl",2)||!owners(r->scope,"F:/scope/open.xl",8)){r->okay=0;break;}xideRenameScopeRelease(r->scope);return r->okay?0:1;}
int main(void)
{
 enum xide_rename_scope_error why=XIDE_RENAME_SCOPE_OK;
 CHECK(!xideEditRequestScope(NULL,0,NULL,NULL,&why)&&why==XIDE_RENAME_SCOPE_ARGUMENT);
 xls_sync* sync=xlsSyncCreateMonotonic(2);xvalue* cat=catalog(1);CHECK(sync&&cat);
 char first[]="F:/scope/one.xl";xide_file entries[]={{.path=first},{.path="F:/scope/two.xl"},{.path="F:/scope/./one.xl"},{.path="F:/scope/other.c"},{.path="F:/scope/shared.xl"}};xide_files files={entries,5,5};
 xide_edit_request_model model[]={{1,1,1,1,0,"F:/scope/project/main.xl","xlang"},{2,1,1,0,0,"F:/scope/open.xl","xlang"},{3,1,0,0,0,"F:/scope/closed.xl","xlang"},{4,1,1,0,0,"F:/scope/c-mode.xl","c"},{5,1,1,1,0,NULL,"xlang"}};roster models={model,5};
 xide_edit_request* p=request(sync,cat,&files,&models);CHECK(p&&xideEditRequestWorkspaceCount(p)==5&&xideEditRequestWorkspaceMatches(p,&files));
 CHECK(!xideEditRequestWorkspaceAt(p,5)&&!xideEditRequestWorkspaceAt(NULL,0)&&!xideEditRequestWorkspaceCount(NULL)&&!xideEditRequestWorkspaceMatches(NULL,&files));
 xide_edit_request_model m={.identity=123};CHECK(!xideEditRequestModelAt(NULL,0,&m)&&!m.identity&&!m.path&&!xideEditRequestModelAt(p,5,&m)&&!xideEditRequestModelAt(p,0,NULL));CHECK(xideEditRequestModelAt(p,1,&m)&&m.identity==2&&!strcmp(m.path,"F:/scope/open.xl"));
 first[9]='X';CHECK(!xideEditRequestWorkspaceMatches(p,&files)&&!strcmp(xideEditRequestWorkspaceAt(p,0),"F:/scope/one.xl"));first[9]='o';CHECK(xideEditRequestWorkspaceMatches(p,&files));
 files.count=4;CHECK(!xideEditRequestWorkspaceMatches(p,&files));files.count=5;
 cancel probe={0};xide_rename_scope* scope=xideEditRequestScope(p,0,cancelled,&probe,&why);CHECK(scope&&why==XIDE_RENAME_SCOPE_OK&&probe.calls);size_t polls=probe.calls;
 CHECK(xideRenameScopeCount(scope)==4&&owners(scope,"F:/scope/project/main.xl",1)&&owners(scope,"F:/scope/shared.xl",1)&&owners(scope,"F:/scope/one.xl",2)&&owners(scope,"F:/scope/two.xl",4)&&owners(scope,"F:/scope/open.xl",8));
 CHECK(owners(scope,"F:/scope/closed.xl",0)&&owners(scope,"F:/scope/c-mode.xl",0)&&owners(scope,"F:/scope/unlisted.xl",0)&&owners(scope,"F:/scope/other.c",0));
 for(size_t i=1;i<=polls;++i){cancel stop={.at=i};xide_rename_scope* bad=xideEditRequestScope(p,0,cancelled,&stop,&why);CHECK(!bad&&why==XIDE_RENAME_SCOPE_CANCELLED&&stop.calls>=i&&!xideEditRequestInvalidated(p));}
 CHECK(!xideEditRequestScope(p,1,NULL,NULL,&why)&&why==XIDE_RENAME_SCOPE_LIMIT);
#ifdef XLS_COMPILER_XRT
 allocationCalls=allocationFailure=0;probing=1;xide_rename_scope* fault=xideEditRequestScope(p,0,NULL,NULL,&why);probing=0;CHECK(fault&&allocationCalls);size_t allocations=allocationCalls;xideRenameScopeRelease(fault);
 for(size_t i=1;i<=allocations;++i){allocationCalls=0;allocationFailure=i;probing=1;fault=xideEditRequestScope(p,0,NULL,NULL,&why);probing=0;CHECK(!fault&&why!=XIDE_RENAME_SCOPE_OK&&allocationCalls>=i);}
 printf("observed scope allocation refusal boundaries: %zu external XRT malloc/calloc calls\n",allocations);
#endif
 /* A project-owned workspace path does not become a standalone role. */
 xide_files onlyProject={entries+4,1,1};xide_edit_request* owned=request(sync,cat,&onlyProject,NULL);CHECK(owned);xide_rename_scope* projectScope=xideEditRequestScope(owned,0,NULL,NULL,&why);CHECK(projectScope&&xideRenameScopeCount(projectScope)==1&&owners(projectScope,"F:/scope/shared.xl",1));xideRenameScopeRelease(projectScope);xideEditRequestRelease(owned);
 /* Actual total-role capacity; refusal cannot return a truncated prefix. */
 xide_file many[65]={0};char paths[65][80];for(size_t i=0;i<65;++i){snprintf(paths[i],sizeof(paths[i]),"F:/scope/root-%zu.xl",i);many[i].path=paths[i];}xide_files boundary={many,63,65};owned=request(sync,cat,&boundary,NULL);CHECK(owned);projectScope=xideEditRequestScope(owned,0,NULL,NULL,&why);CHECK(projectScope&&xideRenameScopeCount(projectScope)==64&&owners(projectScope,paths[62],UINT64_C(1)<<63));xideRenameScopeRelease(projectScope);xideEditRequestRelease(owned);
 boundary.count=64;owned=request(sync,cat,&boundary,NULL);CHECK(owned&&!xideEditRequestScope(owned,0,NULL,NULL,&why)&&why==XIDE_RENAME_SCOPE_LIMIT);xideEditRequestRelease(owned);
 boundary=(xide_files){many,1,65};many[0].path="relative.xl";owned=request(sync,cat,&boundary,NULL);CHECK(owned&&!xideEditRequestScope(owned,0,NULL,NULL,&why)&&why==XIDE_RENAME_SCOPE_ARGUMENT);xideEditRequestRelease(owned);
 many[0].path=NULL;CHECK(!request(sync,cat,&boundary,NULL));boundary=(xide_files){.count=1};CHECK(!request(sync,cat,&boundary,NULL));boundary=(xide_files){.items=(void*)1,.count=65537};CHECK(!request(sync,cat,&boundary,NULL));
 /* Exact two-MiB workspace metadata admission, plus one-byte refusal. */
 xide_file big[8]={0};size_t remaining=2u*1024u*1024u-8*sizeof(const char*);for(size_t i=0;i<8;++i){size_t n=i<7?262145:remaining;big[i].path=malloc(n+1);CHECK(big[i].path);memset(big[i].path,'p',n);big[i].path[n-1]=0;remaining-=n;}CHECK(!remaining);boundary=(xide_files){big,8,8};owned=request(sync,cat,&boundary,NULL);CHECK(owned&&xideEditRequestMetadata(owned)>2u*1024u*1024u&&xideEditRequestMetadata(owned)<3u*1024u*1024u);xideEditRequestRelease(owned);size_t n=strlen(big[7].path);big[7].path[n]='p';big[7].path[n+1]=0;CHECK(!request(sync,cat,&boundary,NULL));for(size_t i=0;i<8;++i)free(big[i].path);
 /* Largest complete list can contain aliases without exhausting role count. */
 xide_file* aliases=calloc(65536,sizeof(*aliases));CHECK(aliases);for(size_t i=0;i<65536;++i)aliases[i].path="F:/scope/one.xl";boundary=(xide_files){aliases,65536,65536};owned=request(sync,cat,&boundary,NULL);CHECK(owned&&xideEditRequestWorkspaceCount(owned)==65536);projectScope=xideEditRequestScope(owned,0,NULL,NULL,&why);CHECK(projectScope&&xideRenameScopeCount(projectScope)==2);xideRenameScopeRelease(projectScope);xideEditRequestRelease(owned);free(aliases);
 cancel invalidate={.at=2,.invalidate=p};CHECK(!xideEditRequestScope(p,0,cancelled,&invalidate,&why)&&why==XIDE_RENAME_SCOPE_CANCELLED&&xideEditRequestInvalidated(p));CHECK(!xideEditRequestScope(p,0,NULL,NULL,&why)&&why==XIDE_RENAME_SCOPE_CANCELLED);
 xideEditRequestRelease(p);xlsSyncDestroy(sync);xrtValueRelease(cat);
 /* Independent immutable scope survives the request, catalog and models. */
 reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(reader){xideRenameScopeRetain(scope),0};threads[i]=xrtThreadCreate(readScope,readers+i,0);CHECK(threads[i]);}xideRenameScopeRelease(scope);for(size_t i=0;i<4;++i){CHECK(xrtThreadWaitFor(threads[i],XIDE_XRT_TIMEOUT_MS(30000))==XWAIT_OK&&readers[i].okay);xrtThreadDestroy(threads[i]);}
 printf("PASS edit request workspace scope: %u checks; all unopened roots, exact model/workspace capture, project ownership, lexical dedup, 64 roles, 65536 paths, exact2MiB plus one, %zu cancellation points, invalidation and detached four-reader lifetime\n",checks,polls);return 0;
}
