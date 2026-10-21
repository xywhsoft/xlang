#include "analysis/worker.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_check_worker_test:%d: %s\n",__LINE__,#value); return 1; } } while(0)
static xls_text* text(const char* s) { return xlsTextCreate(s,strlen(s),2u*1024u*1024u,NULL); }
static void wake(void* user) { xrtAtomic32FetchAdd(user,1,XMEMORY_RELAXED); }
static enum xls_query_state waitResult(xls_query* q,xls_semantic_check** result,xls_check_stats* stats,uint64_t* thread,char* error)
{
    uint64_t until=xrtClock()+15000000;
    do {
        enum xls_query_state state=xlsQueryCheckResult(q,result,stats,thread,error,256);
        if(state!=XLS_QUERY_PENDING) return state;
        xrtSleepUs(500);
    } while(xrtClock()<until);
    return XLS_QUERY_PENDING;
}
int main(void)
{
    xls_session_limits limits={4,2u*1024u*1024u,4u*1024u*1024u};
    xls_worker* worker=xlsWorkerCreate(&limits,1); CHECK(worker);
    char* path=xrtPathAbs("worker-check-memory.xl"); char* expected=path ? xrtStrDup(path) : NULL; CHECK(path && expected);
    char key[]="entry"; const char* good="func main(...) as int\n int result = 7\n return result\nend\n";
    xls_text* body=text(good); CHECK(body);
    xls_analysis_document input={key,path,body,10,1};
    xls_query* q=xlsWorkerCheck(worker,&input,1,key); CHECK(q);
    xatomic32 wakes; xrtAtomic32Init(&wakes,0); CHECK(xlsQuerySetWake(q,wake,&wakes));
    key[0]='z'; path[0]='?'; xlsTextRelease(body); /* Capture owns labels and text references. */
    xls_semantic_check* first=NULL; xls_check_stats stats; uint64_t thread; char error[256];
    CHECK(waitResult(q,&first,&stats,&thread,error)==XLS_QUERY_READY && first && xlsSemanticCheckInfo(first).accepted);
    CHECK(thread && thread!=xrtThreadCurrentId() && stats.builds==1 && !stats.hits && !strcmp(xlsSemanticCheckInfo(first).path,expected));
    xls_analysis_snapshot syntax={0}; CHECK(xlsQueryResult(q,&syntax,NULL,NULL)==XLS_QUERY_FAILED && !syntax.syntax);
    xlsQueryDestroy(q); CHECK(xrtAtomic32Load(&wakes,XMEMORY_RELAXED)==1);
    key[0]='e'; path[0]=expected[0]; body=text(good); CHECK(body); input.text=body;
    for(unsigned i=0;i<100;++i) {
        input.revision=2+i; q=xlsWorkerCheck(worker,&input,1,key); CHECK(q);
        xls_semantic_check* result=NULL;
        CHECK(waitResult(q,&result,&stats,&thread,error)==XLS_QUERY_READY && result==first && stats.builds==1 && stats.hits==i+1);
        xlsSemanticCheckDestroy(result); xlsQueryDestroy(q);
    }
    input.revision=1; q=xlsWorkerCheck(worker,&input,1,key); CHECK(q);
    xls_semantic_check* result=NULL;
    CHECK(waitResult(q,&result,&stats,&thread,error)==XLS_QUERY_FAILED && !result && strstr(error,"revision")); xlsQueryDestroy(q);
    input.revision=102; xls_text* invalid=text("func main(...) as int\n return \"bad\"\nend\n"); CHECK(invalid); input.text=invalid;
    q=xlsWorkerCheck(worker,&input,1,key); CHECK(q);
    CHECK(waitResult(q,&result,&stats,&thread,error)==XLS_QUERY_READY && result && !xlsSemanticCheckInfo(result).accepted && xlsSemanticCheckInfo(result).diagnostics==1 && stats.builds==2);
    xlsSemanticCheckDestroy(result); xlsQueryDestroy(q); xlsTextRelease(invalid);

    /* Actual long semantic/IR work must run on its actor. Another actor can
     * finish a foreground query while a checked compilation is in progress;
     * one queue slot rejects overload and a cancelled pending task stays out
     * of the semantic session. No sleeps or fake analysis inside the job. */
    xstrbuf large; xrtStrBufInit(&large);
    for(unsigned i=0;i<1500;++i) { char line[128]; int size=snprintf(line,sizeof(line),"func helper_%u(int value) as int\n return value + 1\nend\n",i); CHECK(size>0 && xrtStrBufAppend(&large,(xstrview){line,(size_t)size})); }
    CHECK(xrtStrBufAppend(&large,xrtStrView(good)));
    xls_text* heavy=text(large.Data); xrtStrBufFree(&large); CHECK(heavy);
    input.text=heavy; input.revision=103; q=xlsWorkerCheck(worker,&input,1,key); CHECK(q);
    xls_worker_load load; uint64_t until=xrtClock()+1000000;
    do { CHECK(xlsWorkerLoad(worker,&load)); if(load.running) break; xrtSleepUs(500); } while(xrtClock()<until);
    CHECK(load.running==1);
    xls_query* queued=xlsWorkerCheck(worker,&input,1,key); CHECK(queued);
    CHECK(!xlsWorkerCheck(worker,&input,1,key)); xlsQueryCancel(queued);
    xls_worker* foreground=xlsWorkerCreate(&limits,1); CHECK(foreground);
    xls_analysis_document shortInput={"front",expected,body,20,1}; xls_query* front=xlsWorkerSyntax(foreground,&shortInput,1,"front"); CHECK(front);
    until=xrtClock()+1000000;
    enum xls_query_state state;
    do { state=xlsQueryResult(front,&syntax,NULL,NULL); if(state!=XLS_QUERY_PENDING) break; xrtSleepUs(500); } while(xrtClock()<until);
    CHECK(state==XLS_QUERY_READY && xlsSyntaxAccepted(syntax.syntax)); xlsSyntaxRelease(syntax.syntax);
    CHECK(xlsQueryCheckResult(q,&result,NULL,NULL,NULL,0)==XLS_QUERY_PENDING && !result);
    CHECK(xlsQueryCheckResult(front,&result,NULL,NULL,error,sizeof(error))==XLS_QUERY_FAILED && !result);
    xlsQueryDestroy(front); xlsWorkerDestroy(foreground);
    CHECK(waitResult(q,&result,&stats,&thread,error)==XLS_QUERY_READY && result && xlsSemanticCheckInfo(result).accepted && stats.builds==3);
    xlsQueryDestroy(q);
    xls_semantic_check* cancelledResult=NULL;
    CHECK(waitResult(queued,&cancelledResult,NULL,NULL,error)==XLS_QUERY_CANCELLED && !cancelledResult); xlsQueryDestroy(queued);
    xlsWorkerDestroy(worker);
    CHECK(xlsSemanticCheckInfo(first).accepted && xlsSemanticCheckTypedIndex(first) && xlsSemanticIndexCount(xlsSemanticCheckTypedIndex(result))>10000);
    xlsSemanticCheckDestroy(first); xlsSemanticCheckDestroy(result); xlsTextRelease(body); xlsTextRelease(heavy); xrtFree(path); xrtFree(expected);
    printf("analysis_check_worker_test passed: %u checks; XRT semantic/IR actor, actual reuse, retained inputs/results, wake, source-vs-infrastructure errors, independent foreground work, queue bound and cancellation.\n",checks);
    return 0;
}
