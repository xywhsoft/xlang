#include "services/workspace_symbols.h"
#include "analysis/worker_modules.h"
#include <stdio.h>
#include <string.h>
static xls_worker* borrowed;
static unsigned borrowedDestroyed,ownedDestroyed;
static int failConstructor;
static void observedDestroy(xls_worker* actor)
{if(actor){if(actor==borrowed)++borrowedDestroyed;else ++ownedDestroyed;}xlsWorkerDestroy(actor);}
static char* observedDup(const char* text){return failConstructor?NULL:xrtStrDup(text);}
#define xlsWorkerDestroy observedDestroy
#define xrtStrDup observedDup
#include "../src/services/workspace_symbols.c"
#undef xlsWorkerDestroy
#undef xrtStrDup
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_symbols_shared_test:%d:%s\n",__LINE__,#v);return 1;}}while(0)
static int writeText(const char* path,const char* text)
{return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int checked(xls_worker* actor,xls_context* context,const xls_analysis_document* docs,size_t count,const char* target,xls_check_stats* stats,xls_worker_cache_stats* cache)
{
 xls_query* q=xlsWorkerCheckInContext(actor,context,docs,count,target);if(!q)return 0;
 xls_semantic_check* check=NULL;enum xls_query_state state=XLS_QUERY_PENDING;char error[512]={0};
 for(unsigned i=0;i<30000 && state==XLS_QUERY_PENDING;++i){state=xlsQueryCheckResult(q,&check,stats,NULL,error,sizeof(error));if(state==XLS_QUERY_PENDING)xrtSleep(1);}
 int okay=state==XLS_QUERY_READY && check && xlsQuerySessionCache(q,cache);if(!okay)fprintf(stderr,"check: %s\n",error);xlsSemanticCheckDestroy(check);xlsQueryDestroy(q);return okay;
}
static int finish(xide_workspace_symbol_query* q,xide_workspace_symbol_work* work)
{
 enum xide_workspace_symbol_state state=XIDE_WORKSPACE_SYMBOL_PENDING;char error[512]={0};if(!q)return 0;
 for(unsigned i=0;i<30000 && state==XIDE_WORKSPACE_SYMBOL_PENDING;++i){state=xideWorkspaceSymbolsResult(q,NULL,work,error,sizeof(error));if(state==XIDE_WORKSPACE_SYMBOL_PENDING)xrtSleep(1);}
 if(state!=XIDE_WORKSPACE_SYMBOL_READY)fprintf(stderr,"refresh: %s\n",error);
 return state==XIDE_WORKSPACE_SYMBOL_READY;
}
int main(void)
{
 char *directory=xrtPathAbs("build/workspace-symbols-shared/中文😀"),*entry=xrtPathJoin(directory,"main.xl"),*helper=xrtPathJoin(directory,"helper.xl"),*orphan=xrtPathJoin(directory,"orphan.xl"),*compiler=xrtPathJoin(directory,"absent-xl.exe");CHECK(directory && entry && helper && orphan && compiler && xrtDirCreateAll(directory));
 const char* body="import helper\nfunc main(...) as int\n return helper.value()\nend\n";
 const char* library="class Payload\n int number\nend\nexport func value() as int\n return 1\nend\n";
 const char* dirty="class Payload\n int amount\nend\nexport func value() as int\n return 2\nend\n";
 CHECK(writeText(entry,body) && writeText(helper,library) && writeText(orphan,"func orphanWork() as int\n return 7\nend\nfunc main(...) as int\n return orphanWork()\nend\n"));
 const char* members[]={entry,helper,orphan};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project="shared.xlp",.configuration="Debug",.entry=entry,.directory=directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,2};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xls_context* scopes[]={context};xide_toolchains* cache=xideToolchainsCreate();xls_module_store* modules=xlsModuleStoreCreate(NULL);borrowed=xlsWorkerCreateWithModules(NULL,8,NULL,modules);CHECK(context && cache && modules && borrowed);
 CHECK(!xideWorkspaceSymbolsCreateWithActor(cache,compiler,modules,NULL));
 failConstructor=1;CHECK(!xideWorkspaceSymbolsCreateWithActor(cache,compiler,modules,borrowed) && !borrowedDestroyed);failConstructor=0;
 xls_text* text=xlsTextCreate(body,strlen(body),1024*1024,NULL);xls_analysis_document doc={"entry",entry,text,1,1};xls_check_stats stats={0};xls_worker_cache_stats session={0};CHECK(text && checked(borrowed,context,&doc,1,"entry",&stats,&session) && stats.builds==1 && !stats.hits && session.sessions==1);
 xide_workspace_symbols* store=xideWorkspaceSymbolsCreateWithActor(cache,compiler,modules,borrowed);CHECK(store && !store->actor && store->foreground==borrowed);xide_workspace_symbol_work work={0};
 xls_projection_stats projectionBefore={0},projectionAfter={0};xlsProjectionStoreStats(xlsWorkerProjectionStore(borrowed),&projectionBefore);
 xide_workspace_symbol_query* q=xideWorkspaceSymbolsRefresh(store,scopes,1,&doc,1);CHECK(finish(q,&work) && work.captured==1 && work.covered==1 && work.index.rows==4 && work.analysisBuilds==1 && !work.analysisHits && work.analysisSnapshots==1 && !work.primaryFallbacks && !store->actor);xideWorkspaceSymbolsQueryDestroy(q);
 xlsProjectionStoreStats(xlsWorkerProjectionStore(borrowed),&projectionAfter);CHECK(projectionAfter.published==projectionBefore.published && projectionAfter.retired==projectionBefore.retired && projectionAfter.replaced==projectionBefore.replaced && projectionAfter.entries==projectionBefore.entries);
 CHECK(checked(borrowed,context,&doc,1,"entry",&stats,&session) && stats.builds==1 && stats.hits>=1 && session.sessions==1);
 /* Equivalent independent context must reuse the full foreground context. */
 xls_context* equivalent=xlsContextCreate(&spec,16u*1024u*1024u,NULL);CHECK(equivalent);scopes[0]=equivalent;q=xideWorkspaceSymbolsRefresh(store,scopes,1,&doc,1);CHECK(finish(q,&work) && work.reused==1 && !work.captured);xideWorkspaceSymbolsQueryDestroy(q);xlsContextRelease(equivalent);scopes[0]=context;
 xideWorkspaceSymbolsDestroy(store);CHECK(!borrowedDestroyed && !ownedDestroyed && checked(borrowed,context,&doc,1,"entry",&stats,&session) && stats.builds==1);
 xlsWorkerDestroy(borrowed);borrowed=xlsWorkerCreateWithModules(NULL,8,NULL,modules);CHECK(borrowed);
 /* No open editor: temporary fallback leaves foreground history untouched. */
 store=xideWorkspaceSymbolsCreateWithActor(cache,compiler,modules,borrowed);q=xideWorkspaceSymbolsRefresh(store,scopes,1,NULL,0);CHECK(finish(q,&work) && work.captured==1 && work.analysisBuilds==1 && work.primaryFallbacks==1 && !work.analysisSnapshots && !store->actor);xideWorkspaceSymbolsQueryDestroy(q);
 CHECK(checked(borrowed,context,NULL,0,NULL,&stats,&session) && stats.builds==1 && !stats.hits && session.sessions==1);
 /* Unsaved imported text is checked, not inherited from the old disk proof. */
 /* Match the host's captured lexical spelling for non-entry overlays too.
    The core deliberately treats differently spelled batch paths as new inputs. */
 char* helperWire=xidePathResolveLexical(directory,helper);xls_text* changed=xlsTextCreate(dirty,strlen(dirty),1024*1024,NULL);xls_analysis_document overlay={"helper",helperWire,changed,2,1};CHECK(helperWire && changed && checked(borrowed,context,&overlay,1,NULL,&stats,&session));uint64_t builds=stats.builds;
 q=xideWorkspaceSymbolsSubmitLsp(store,scopes,1,&overlay,1,"Payload amount",200,0);CHECK(finish(q,&work) && work.captured==1 && work.analysisBuilds==builds && !store->actor);xvalue* rows=xideWorkspaceSymbolsTakeLsp(q);CHECK(rows && xrtValueCount(rows)==1);xrtValueRelease(rows);xideWorkspaceSymbolsQueryDestroy(q);
 q=xideWorkspaceSymbolsSubmitLsp(store,scopes,1,&overlay,1,"Payload number",200,0);CHECK(finish(q,&work));rows=xideWorkspaceSymbolsTakeLsp(q);CHECK(rows && !xrtValueCount(rows));xrtValueRelease(rows);xideWorkspaceSymbolsQueryDestroy(q);
 xideWorkspaceSymbolsDestroy(store);CHECK(!borrowedDestroyed);xlsWorkerDestroy(borrowed);borrowed=xlsWorkerCreateWithModules(NULL,8,NULL,modules);CHECK(borrowed);
 /* Member traversal gets its own actor; foreground keeps only the full context. */
 spec.lists[XLS_CONTEXT_MEMBERS].count=3;xls_context* complete=xlsContextCreate(&spec,16u*1024u*1024u,NULL);scopes[0]=complete;CHECK(complete && checked(borrowed,complete,NULL,0,NULL,&stats,&session));store=xideWorkspaceSymbolsCreateWithActor(cache,compiler,modules,borrowed);xls_worker_load before={0},after={0};CHECK(store && xlsWorkerLoad(borrowed,&before));
 q=xideWorkspaceSymbolsRefresh(store,scopes,1,NULL,0);CHECK(finish(q,&work) && work.captured==2 && work.covered==1 && work.index.rows==6 && store->actor && store->actor!=borrowed);xideWorkspaceSymbolsQueryDestroy(q);CHECK(xlsWorkerLoad(borrowed,&after) && after.submitted==before.submitted+1);
 CHECK(checked(borrowed,complete,NULL,0,NULL,&stats,&session) && stats.builds==1 && stats.hits>=1 && session.sessions==1);
 xideWorkspaceSymbolsDestroy(store);CHECK(!borrowedDestroyed && ownedDestroyed==2);CHECK(checked(borrowed,complete,NULL,0,NULL,&stats,&session) && stats.builds==1);
 xlsWorkerDestroy(borrowed);borrowed=NULL;xlsModuleStoreRelease(modules);xideToolchainsDestroy(cache);xlsContextRelease(context);xlsContextRelease(complete);xlsTextRelease(text);xlsTextRelease(changed);free(helperWire);xrtFree(directory);xrtFree(entry);xrtFree(helper);xrtFree(orphan);xrtFree(compiler);
 printf("workspace_symbols_shared_test passed: %u checks;exact full-context foreground reuse,zero-open isolated fallback,unsaved import proof,lazy isolated orphan actor,constructor failure and borrowed/owned lifetimes.\n",checks);return 0;
}
