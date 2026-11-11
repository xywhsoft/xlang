/* Compiled by check_lsp_analysis_lease_native.py with exact product units.
 * Private adapters expose only actual actor pools and XRT's real finalizer
 * submission; no replacement scheduler or guessed running counters. */
#define main xideServerMain
#include "server/xlang_ls.c"
#undef main
#include "analysis/pool_idle.h"

xtaskpool* xideTestWorkerPool(xls_worker*);
xtaskpool* xideTestDiagnosticPool(xide_diagnostic_store*);
xtaskpool* xideTestReferencePool(xide_project_references*,size_t);
xtaskpool* xideTestWorkspacePool(xide_workspace_symbols*,size_t);
void xideTestPostFinalizer(xtaskpool*,void (*)(void*),void*);

static int assertions;
#define REQUIRE(c) do{++assertions;if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
typedef struct blocked {HANDLE entered,release;volatile LONG timeout;} blocked;
static void blockedInit(blocked* b){memset(b,0,sizeof(*b));b->entered=CreateEventW(NULL,TRUE,FALSE,NULL);b->release=CreateEventW(NULL,TRUE,FALSE,NULL);REQUIRE(b->entered && b->release);}
static void blockedRun(void* p){blocked* b=p;SetEvent(b->entered);if(WaitForSingleObject(b->release,10000)!=WAIT_OBJECT_0)InterlockedExchange(&b->timeout,1);}
static void blockedUnit(blocked* b){REQUIRE(!b->timeout);CloseHandle(b->entered);CloseHandle(b->release);}
static xtaskoutcome task(xcancel* c,void* p,xtaskvalue* out){(void)c;(void)out;blockedRun(p);return XTASK_SUCCESS;}
static void cleanup(void* p,void* u){(void)p;blockedRun(u);}
static void request(server* s,int64 token,int64 id){xvalue* params=xrtValueObject();xvalue* rid=xrtValueInt(id);REQUIRE(put(params,"token",xrtValueInt(token)));REQUIRE(analysisLeaseDispatch(s,"xlang/suspendAnalysis",rid,params));xrtValueRelease(params);xrtValueRelease(rid);}
static void resume(server* s,int64 token,int64 id){xvalue* params=xrtValueObject();xvalue* rid=xrtValueInt(id);REQUIRE(put(params,"token",xrtValueInt(token)));REQUIRE(analysisLeaseDispatch(s,"xlang/resumeAnalysis",rid,params));xrtValueRelease(params);xrtValueRelease(rid);}
static void awaitQuiet(server* s){ULONGLONG end=GetTickCount64()+10000;while(!analysisLeaseQuiet(s)){REQUIRE(GetTickCount64()<end);Sleep(1);}}
int main(int argc,char** argv)
{
 REQUIRE(argc==2);char* log=xidePathResolveLexical(argv[1],"native-frames.bin");REQUIRE(log);wchar_t* native=xidePathNativeWide(log);REQUIRE(native);
 HANDLE output=CreateFileW(native,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);REQUIRE(output!=INVALID_HANDLE_VALUE);free(native);free(log);HANDLE old=GetStdHandle(STD_OUTPUT_HANDLE);REQUIRE(SetStdHandle(STD_OUTPUT_HANDLE,output));
 server s={0};InitializeCriticalSection(&s.lock);s.event=CreateEventW(NULL,FALSE,FALSE,NULL);s.pending=-1;s.initialized=1;
 s.analysis=xlsWorkerCreate(NULL,8);s.semanticAnalysis=xlsWorkerCreate(NULL,8);s.cursorAnalysis=xlsWorkerCreate(NULL,8);s.signatureAnalysis=xlsWorkerCreate(NULL,8);
 s.cache=xideToolchainsCreate();s.diagnostics=xideDiagnosticStoreCreate(NULL);s.references=xideProjectReferencesCreate(s.cache,"absent-xl.exe",NULL);s.workspaceSymbols=xideWorkspaceSymbolsCreate(s.cache,"absent-xl.exe",NULL);
 REQUIRE(s.event && s.analysis && s.semanticAnalysis && s.cursorAnalysis && s.signatureAnalysis && s.cache && s.diagnostics && s.references && s.workspaceSymbols);REQUIRE(!xlsWorkerIdle(NULL));REQUIRE(xideDiagnosticStoreIdle(NULL) && xideProjectReferencesIdle(NULL) && xideWorkspaceSymbolsIdle(NULL));awaitQuiet(&s);
 xtaskpool* pools[16]={xideTestWorkerPool(s.analysis),xideTestWorkerPool(s.semanticAnalysis),xideTestWorkerPool(s.cursorAnalysis),xideTestWorkerPool(s.signatureAnalysis),xideTestDiagnosticPool(s.diagnostics)};size_t count=5;
 for(size_t i=0;i<6;++i){xtaskpool* p=xideTestReferencePool(s.references,i);REQUIRE(p);pools[count++]=p;}
 for(size_t i=0;i<2;++i){xtaskpool* p=xideTestWorkspacePool(s.workspaceSymbols,i);REQUIRE(p);pools[count++]=p;}
 REQUIRE(count==13);
 for(size_t i=0;i<count;++i){
  blocked b;blockedInit(&b);xideTestPostFinalizer(pools[i],blockedRun,&b);REQUIRE(WaitForSingleObject(b.entered,10000)==WAIT_OBJECT_0);
  xtaskpoolstats stats={0};REQUIRE(xrtTaskPoolGet(pools[i],&stats) && !stats.Queued && !stats.Running);REQUIRE(!xlsRuntimeTaskPoolIdle(pools[i]));
  request(&s,(int64)i+1,(int64)i+100);REQUIRE(s.analysisLeaseToken==(int64)i+1 && !s.analysisLeaseHeld);REQUIRE(!analysisLeaseQuiet(&s));analysisLeasePoll(&s);REQUIRE(!s.analysisLeaseHeld && s.analysisLeaseId);
  SetEvent(b.release);awaitQuiet(&s);analysisLeasePoll(&s);REQUIRE(s.analysisLeaseHeld && !s.analysisLeaseId);resume(&s,(int64)i+1,(int64)i+200);REQUIRE(!s.analysisLeaseToken);blockedUnit(&b);
 }
 /* Real task procedure and data destruction remain busy, too. */
 blocked running,retiring;blockedInit(&running);blockedInit(&retiring);xtaskargs args={.Destroy=cleanup,.DestroyData=&retiring};xfuture* future=xrtTaskSubmit(pools[0],task,&running,&args);REQUIRE(future);REQUIRE(WaitForSingleObject(running.entered,10000)==WAIT_OBJECT_0);REQUIRE(!xlsWorkerIdle(s.analysis));SetEvent(running.release);REQUIRE(WaitForSingleObject(retiring.entered,10000)==WAIT_OBJECT_0);REQUIRE(!xlsWorkerIdle(s.analysis));SetEvent(retiring.release);awaitQuiet(&s);xrtFutureDestroy(future);blockedUnit(&running);blockedUnit(&retiring);
 /* Pending cancellation releases no acknowledged lease. Exact resume can
    abandon a still-pending token; reuse and old request cancellation cannot. */
 blocked tail;blockedInit(&tail);xideTestPostFinalizer(pools[4],blockedRun,&tail);REQUIRE(WaitForSingleObject(tail.entered,10000)==WAIT_OBJECT_0);request(&s,20,400);analysisLeasePoll(&s);REQUIRE(!s.analysisLeaseHeld);
 xvalue* p=xrtValueObject();put(p,"id",xrtValueInt(400));REQUIRE(analysisLeaseDispatch(&s,"$/cancelRequest",NULL,p));REQUIRE(!s.analysisLeaseToken);xrtValueRelease(p);
 request(&s,20,401);REQUIRE(!s.analysisLeaseToken);request(&s,21,402);resume(&s,20,403);REQUIRE(s.analysisLeaseToken==21);resume(&s,21,404);REQUIRE(!s.analysisLeaseToken);SetEvent(tail.release);awaitQuiet(&s);blockedUnit(&tail);
 request(&s,22,405);analysisLeasePoll(&s);REQUIRE(s.analysisLeaseHeld);p=xrtValueObject();put(p,"id",xrtValueInt(405));analysisLeaseDispatch(&s,"$/cancelRequest",NULL,p);xrtValueRelease(p);REQUIRE(s.analysisLeaseHeld && s.analysisLeaseToken==22);
 /* Even a due sweep and an unseen/ready idle index may not submit work. */
 s.automaticDiagnostics=1;s.diagnosticSweepAt=GetTickCount64();diagnosticPoll(&s);REQUIRE(!s.diagnosticQuery);
 s.workspaceRefresh.enabled=s.workspaceRefresh.ready=1;s.workspaceRefresh.seen=0;pollWorkspaceRefresh(&s);REQUIRE(!s.workspaceRefresh.query && !s.workspaceRefresh.seen);
 /* Deferred and reader queues have ONE bound; moving a notification cannot
    open an admission slot. Resume splices exact DOMs in original order. */
 for(int i=0;i<200;++i){xvalue* m=xrtValueObject();put(m,"jsonrpc",xrtValueString(sv("2.0")));put(m,"method",xrtValueString(sv("textDocument/didChange")));put(m,"params",xrtValueInt(i));REQUIRE(receive(m,&s));xrtValueRelease(m);int held=0;REQUIRE(!analysisLeaseNext(&s,&held) && held);}
 REQUIRE(s.queueCount==200 && s.queueInputs==200 && s.deferredCount==200);size_t bytes=s.queueBytes;
 xvalue* query=xrtValueObject();put(query,"jsonrpc",xrtValueString(sv("2.0")));put(query,"method",xrtValueString(sv("xlang/inputState")));put(query,"id",xrtValueInt(999));
 for(int i=0;i<56;++i){REQUIRE(receive(query,&s));}
 REQUIRE(s.queueCount==256 && s.queueBytes>bytes);REQUIRE(!receive(query,&s));xrtValueRelease(query);resume(&s,22,406);REQUIRE(!s.deferredCount && s.queueCount==256);
 for(int i=0;i<256;++i){int held=0;message* m=analysisLeaseNext(&s,&held);REQUIRE(m && !held);if(i<200){int64 n=-1;REQUIRE(xrtValueGetInt(get(m->value,"params"),&n) && n==i);}xrtValueRelease(m->value);free(m);}REQUIRE(!s.queueCount && !s.queueBytes && !s.queueInputs);
 /* Byte quota is also shared, independently of the frame count. */
 request(&s,23,407);s.queueBytes=QUEUE_LIMIT;query=xrtValueObject();put(query,"jsonrpc",xrtValueString(sv("2.0")));put(query,"method",xrtValueString(sv("initialized")));REQUIRE(!receive(query,&s));xrtValueRelease(query);s.queueBytes=0;resume(&s,23,408);
 REQUIRE(!s.writeFailed);xideWorkspaceSymbolsDestroy(s.workspaceSymbols);xideProjectReferencesDestroy(s.references);xideDiagnosticStoreDestroy(s.diagnostics);xlsWorkerDestroy(s.analysis);xlsWorkerDestroy(s.semanticAnalysis);xlsWorkerDestroy(s.cursorAnalysis);xlsWorkerDestroy(s.signatureAnalysis);xideToolchainsDestroy(s.cache);CloseHandle(s.event);DeleteCriticalSection(&s.lock);REQUIRE(SetStdHandle(STD_OUTPUT_HANDLE,old));CloseHandle(output);
 fprintf(stderr,"PASS analysis lease: %d assertions; 13 actual finalizer tails, real task and cleanup, aggregate queue, token/cancellation fences\n",assertions);return 0;
}
