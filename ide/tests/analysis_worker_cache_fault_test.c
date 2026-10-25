#include "analysis/check_session.h"
#include "analysis/completion_session.h"
#include "analysis/signature_session.h"
#include <xrt.h>
static xatomic32 faultKind,faultCalls,faultStop;
static xls_check_session* faultCreateCheck(const xls_session_limits* limits)
{return xrtAtomic32Load(&faultKind,XMEMORY_ACQUIRE)==1 ? NULL : xlsCheckSessionCreate(limits);}
static xls_completion_session* faultCreateCompletion(const xls_session_limits* limits)
{return xrtAtomic32Load(&faultKind,XMEMORY_ACQUIRE)==2 ? NULL : xlsCompletionSessionCreate(limits);}
static xls_signature_session* faultCreateSignature(const xls_session_limits* limits)
{return xrtAtomic32Load(&faultKind,XMEMORY_ACQUIRE)==3 ? NULL : xlsSignatureSessionCreate(limits);}
static bool faultRequested(const xcancel* cancel)
{
 uint32 stop=xrtAtomic32Load(&faultStop,XMEMORY_ACQUIRE),at=xrtAtomic32FetchAdd(&faultCalls,1,XMEMORY_RELAXED)+1;
 return xrtCancelRequested(cancel) || (stop && at>=stop);
}
/* Private exact worker,real parser/semantics/IR/sessions. Only optional session
 * creation and its actual cancellation predicate are controlled by the test. */
#define xlsCheckSessionCreate faultCreateCheck
#define xlsCompletionSessionCreate faultCreateCompletion
#define xlsSignatureSessionCreate faultCreateSignature
#define xrtCancelRequested faultRequested
#define xlsWorkerCreate faultWorkerCreate
#define xlsWorkerCreateWithCache faultWorkerCreateWithCache
#define xlsWorkerCreateWithModules faultWorkerCreateWithModules
#define xlsWorkerDestroy faultWorkerDestroy
#define xlsWorkerLoad faultWorkerLoad
#define xlsWorkerProjectionStore faultWorkerProjectionStore
#define xlsWorkerSyntax faultWorkerSyntax
#define xlsWorkerFolding faultWorkerFolding
#define xlsWorkerCheck faultWorkerCheck
#define xlsWorkerCheckInContext faultWorkerCheckInContext
#define xlsWorkerCompletion faultWorkerCompletion
#define xlsWorkerCompletionInContext faultWorkerCompletionInContext
#define xlsWorkerSignatureInContext faultWorkerSignatureInContext
#define xlsQueryCancel faultQueryCancel
#define xlsQuerySetWake faultQuerySetWake
#define xlsQueryDestroy faultQueryDestroy
#define xlsQueryResult faultQueryResult
#define xlsQueryFolding faultQueryFolding
#define xlsQueryCheckResult faultQueryCheckResult
#define xlsQueryCompletionResult faultQueryCompletionResult
#define xlsQuerySignatureResult faultQuerySignatureResult
#define xlsQuerySessionCache faultQuerySessionCache
#include "analysis/worker.c"
#define main ordinaryCacheFixtureMain
#include "analysis_worker_cache_test.c"
#undef main

static int factoryFailures(void)
{
 fixture f={0};CHECK(setup(&f));
 for(unsigned kind=0;kind<3;++kind){
  xrtAtomic32Store(&faultKind,0,XMEMORY_RELEASE);xls_worker* w=faultWorkerCreate(NULL,4);CHECK(w);
  fixture_result a=query(w,kind,NULL,f.docs,3,"a","a");CHECK(a.state==1);
  xrtAtomic32Store(&faultKind,kind+1,XMEMORY_RELEASE);fixture_result b=query(w,kind,NULL,f.docs,3,"b","b");
  CHECK(b.state==1 && b.cache.allocationFallbacks==1 && b.cache.evictions==1 && b.cache.sessions==1 && builds(&b,kind)==2);
  CHECK(parity(&b,kind,NULL,f.docs,3,"b","b"));
  xrtAtomic32Store(&faultKind,0,XMEMORY_RELEASE);fixture_result r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(r.state==1 && !sameResult(&a,&r,kind) && builds(&r,kind)==3 && r.cache.sessions==2);releaseResult(&r);
  r=query(w,kind,NULL,f.docs,3,"b","b");CHECK(r.state==1 && sameResult(&b,&r,kind) && builds(&r,kind)==3);releaseResult(&r);
  faultWorkerDestroy(w);CHECK(parity(&a,kind,NULL,f.docs,3,"a","a"));releaseResult(&a);releaseResult(&b);
 }
 clear(&f);return 1;
}
static int cancellationGates(void)
{
 fixture f={0};CHECK(setup(&f));char* changed=xrtStrDup(sourceA);CHECK(changed);char* digit=strstr(changed,"pick_A(7");CHECK(digit);digit[7]='8';
 xls_text* edited=xlsTextCreate(changed,strlen(changed),1024*1024,NULL);xrtFree(changed);CHECK(edited);
 unsigned totalGates=0;
 for(unsigned kind=0;kind<3;++kind)for(unsigned size=0;size<2;++size)for(unsigned variant=0;variant<3;++variant){
  size_t slots=size ? 8 : 1;const char* target=variant ? "a" : "c";
  xls_analysis_document attempt[3];memcpy(attempt,f.docs,sizeof(attempt));size_t changedIndex=variant==1 ? 0 : 2;attempt[changedIndex].revision=2;if(variant)attempt[changedIndex].text=edited;
  xrtAtomic32Store(&faultStop,0,XMEMORY_RELEASE);xls_worker* w=faultWorkerCreateWithCache(NULL,4,&(xls_worker_cache_limits){slots,64u*1024u*1024u});CHECK(w);
  fixture_result seed=query(w,kind,NULL,f.docs,3,"a","a");CHECK(seed.state==1);releaseResult(&seed);
  if(size){seed=query(w,kind,NULL,f.docs,3,"b","b");CHECK(seed.state==1);releaseResult(&seed);}
  xrtAtomic32Store(&faultCalls,0,XMEMORY_RELEASE);fixture_result r=query(w,kind,NULL,attempt,3,target,target);CHECK(r.state==1);unsigned gates=xrtAtomic32Load(&faultCalls,XMEMORY_ACQUIRE);CHECK(gates>3);releaseResult(&r);faultWorkerDestroy(w);totalGates+=gates;
  for(unsigned stop=1;stop<=gates;++stop){
   w=faultWorkerCreateWithCache(NULL,4,&(xls_worker_cache_limits){slots,64u*1024u*1024u});CHECK(w);
   fixture_result a=query(w,kind,NULL,f.docs,3,"a","a"),b={0};CHECK(a.state==1);
   if(size){b=query(w,kind,NULL,f.docs,3,"b","b");CHECK(b.state==1);}
   xrtAtomic32Store(&faultCalls,0,XMEMORY_RELEASE);xrtAtomic32Store(&faultStop,stop,XMEMORY_RELEASE);
   r=query(w,kind,NULL,attempt,3,target,target);CHECK(r.state==XLS_QUERY_CANCELLED && !r.check && !r.completion && !r.signature && !r.cache.sessions);releaseResult(&r);
   xrtAtomic32Store(&faultStop,0,XMEMORY_RELEASE);
   r=query(w,kind,NULL,f.docs,3,"a","a");CHECK(r.state==1 && sameResult(&a,&r,kind) && r.cache.sessions==(size ? 2u : 1u) && !r.cache.evictions);
   CHECK(parity(&r,kind,NULL,f.docs,3,"a","a"));releaseResult(&r);
   if(size){r=query(w,kind,NULL,f.docs,3,"b","b");CHECK(r.state==1 && sameResult(&b,&r,kind));releaseResult(&r);}
   faultWorkerDestroy(w);releaseResult(&a);releaseResult(&b);
  }
 }
 printf("Worker cache cancellation gates=%u across3kinds x2capacities x3paths;including final worker gate after native session commit\n",totalGates);
 xlsTextRelease(edited);clear(&f);return 1;
}
int main(void)
{
 xrtAtomic32Init(&faultKind,0);xrtAtomic32Init(&faultCalls,0);xrtAtomic32Init(&faultStop,0);
 if(!factoryFailures() || !cancellationGates())return 1;
 printf("analysis_worker_cache_fault_test passed: %u checks;real session-allocation fallback and all calibrated worker cancellation gates preserve accepted roots,input revisions and complete fresh facts.\n",checks);return 0;
}
