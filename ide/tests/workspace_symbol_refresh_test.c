#include "services/workspace_symbols.h"
#include "protocol/workspace_symbol_wire.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
static volatile LONG queries,wires,gate;
static HANDLE entered,resume;
static xide_workspace_symbol_result* observedQuery(xide_workspace_symbol_snapshot* s,const char* q,size_t n,int (*stop)(void*),void* u,xide_model_error* e)
{InterlockedIncrement(&queries);return xideWorkspaceSymbolSnapshotQuery(s,q,n,stop,u,e);}
static xvalue* observedWire(const xide_workspace_symbol_result* r,uint32_t k,int (*stop)(void*),void* u,xide_model_error* e)
{InterlockedIncrement(&wires);return xideWorkspaceSymbolWire(r,k,stop,u,e);}
static int observedCommit(xide_workspace_symbol_update* update,xide_model_error* e)
{if(InterlockedCompareExchange(&gate,2,1)==1){SetEvent(entered);WaitForSingleObject(resume,10000);}return xideWorkspaceSymbolIndexCommit(update,e);}
#define xideWorkspaceSymbolSnapshotQuery observedQuery
#define xideWorkspaceSymbolWire observedWire
#define xideWorkspaceSymbolIndexCommit observedCommit
#include "../src/services/workspace_symbols.c"
#undef xideWorkspaceSymbolSnapshotQuery
#undef xideWorkspaceSymbolWire
#undef xideWorkspaceSymbolIndexCommit
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_symbol_refresh_test:%d:%s\n",__LINE__,#v);return 1;}}while(0)
static enum xide_workspace_symbol_state finish(xide_workspace_symbol_query* q,xide_workspace_symbol_work* work)
{if(!q)return XIDE_WORKSPACE_SYMBOL_FAILED;enum xide_workspace_symbol_state state=XIDE_WORKSPACE_SYMBOL_PENDING;for(unsigned i=0;i<30000 && state==XIDE_WORKSPACE_SYMBOL_PENDING;++i){state=xideWorkspaceSymbolsResult(q,NULL,work,NULL,0);if(state==XIDE_WORKSPACE_SYMBOL_PENDING)xrtSleep(1);}return state;}
static int writeText(const char* path,const char* text){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
int main(void)
{
 char *directory=xrtPathAbs("build/workspace-symbol-refresh/中文😀"),*entry=xrtPathJoin(directory,"main.xl"),*helper=xrtPathJoin(directory,"helper.xl"),*orphan=xrtPathJoin(directory,"orphan.xl"),*compiler=xrtPathJoin(directory,"absent-xl.exe"),*project=xrtPathJoin(directory,"project.xlp");CHECK(directory && entry && helper && orphan && compiler && project && xrtDirCreateAll(directory));
 const char* entryBody="import helper\nfunc main(...) as int\n return helper.value()\nend\n";
 const char* library="class Payload\n int number\nend\nexport func value() as int\n return 1\nend\n";
 const char* dirty="class Payload\n int amount\nend\nexport func value() as int\n return 2\nend\n";
 CHECK(writeText(entry,entryBody) && writeText(helper,library) && writeText(orphan,"func orphanWork() as int\n return 7\nend\nfunc main(...) as int\n return orphanWork()\nend\n"));
 const char* members[]={entry,helper,orphan};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="Debug",.entry=entry,.directory=directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,3};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xls_context* scopes[]={context};xide_toolchains* cache=xideToolchainsCreate();xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(cache,compiler,NULL);CHECK(context && cache && store);
 xide_workspace_symbol_work work={0};xide_workspace_symbol_query* q=xideWorkspaceSymbolsRefresh(store,NULL,0,NULL,0);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_READY && !work.roots && !work.index.rows && !queries && !wires);CHECK(!xideWorkspaceSymbolsTake(q) && !xideWorkspaceSymbolsTakeLsp(q));xideWorkspaceSymbolsQueryDestroy(q);
 q=xideWorkspaceSymbolsRefresh(store,scopes,1,NULL,0);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_READY && work.roots==3 && work.captured==2 && work.covered==1 && work.index.rows==6 && !queries && !wires);uint64_t generation=work.index.generation;xideWorkspaceSymbolsQueryDestroy(q);
 q=xideWorkspaceSymbolsSubmitLsp(store,scopes,1,NULL,0,"Payload number",200,0);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_READY && !work.captured && work.reused==2 && queries==1 && wires==1);xvalue* rows=xideWorkspaceSymbolsTakeLsp(q);CHECK(rows && xrtValueCount(rows)==1);xrtValueRelease(rows);xideWorkspaceSymbolsQueryDestroy(q);
 xls_text* text=xlsTextCreate(dirty,strlen(dirty),1024*1024,NULL);xls_analysis_document overlay={"helper",helper,text,1,1};CHECK(text);q=xideWorkspaceSymbolsRefresh(store,scopes,1,&overlay,1);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_READY && work.captured==1 && work.reused==1 && queries==1 && wires==1 && work.index.generation>generation);xideWorkspaceSymbolsQueryDestroy(q);
 q=xideWorkspaceSymbolsSubmitLsp(store,scopes,1,&overlay,1,"Payload amount",200,0);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_READY && !work.captured && work.reused==2);rows=xideWorkspaceSymbolsTakeLsp(q);CHECK(rows && xrtValueCount(rows)==1);xrtValueRelease(rows);xideWorkspaceSymbolsQueryDestroy(q);LONG oldQueries=queries,oldWires=wires;
 q=xideWorkspaceSymbolsRefresh(store,scopes,1,NULL,0);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_READY && work.captured==1 && work.reused==1 && queries==oldQueries && wires==oldWires);xideWorkspaceSymbolsQueryDestroy(q);
 CHECK(writeText(entry,"func main(...) as int\n return \"invalid\"\nend\n"));q=xideWorkspaceSymbolsRefresh(store,scopes,1,NULL,0);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_FAILED && !xideWorkspaceSymbolsTake(q) && !xideWorkspaceSymbolsTakeLsp(q) && queries==oldQueries && wires==oldWires);xideWorkspaceSymbolsQueryDestroy(q);
 q=xideWorkspaceSymbolsSubmitLsp(store,scopes,1,NULL,0,"Payload number",200,0);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_FAILED && !xideWorkspaceSymbolsTakeLsp(q));xideWorkspaceSymbolsQueryDestroy(q);CHECK(writeText(entry,entryBody));
 entered=CreateEventW(NULL,TRUE,FALSE,NULL);resume=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(entered && resume);InterlockedExchange(&gate,1);q=xideWorkspaceSymbolsRefresh(store,scopes,1,NULL,0);CHECK(q && WaitForSingleObject(entered,30000)==WAIT_OBJECT_0);xideWorkspaceSymbolsCancel(q);CHECK(xideWorkspaceSymbolsResult(q,NULL,NULL,NULL,0)!=XIDE_WORKSPACE_SYMBOL_READY);CHECK(SetEvent(resume));CHECK(finish(q,NULL)==XIDE_WORKSPACE_SYMBOL_CANCELLED);xideWorkspaceSymbolsQueryDestroy(q);
 q=xideWorkspaceSymbolsRefresh(store,scopes,1,NULL,0);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_READY && work.index.rows==6);CHECK(queries==oldQueries && wires==oldWires);xideWorkspaceSymbolsQueryDestroy(q);
 q=xideWorkspaceSymbolsRefresh(store,NULL,0,NULL,0);CHECK(finish(q,&work)==XIDE_WORKSPACE_SYMBOL_READY && !work.index.rows && !work.roots && !xideWorkspaceSymbolsTake(q) && !xideWorkspaceSymbolsTakeLsp(q));xideWorkspaceSymbolsQueryDestroy(q);
 xideWorkspaceSymbolsDestroy(store);xideToolchainsDestroy(cache);xlsContextRelease(context);xlsTextRelease(text);xrtFree(directory);xrtFree(entry);xrtFree(helper);xrtFree(orphan);xrtFree(compiler);xrtFree(project);CloseHandle(entered);CloseHandle(resume);
 printf("workspace_symbol_refresh_test passed: %u checks;actual full native refresh without result scan/wire,zero-open members,query reuse,overlay close,strict source failure/recovery,deterministic pre-commit cancellation and empty-cohort release.\n",checks);return 0;
}
