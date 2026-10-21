#include "analysis/worker.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_worker_test:%d: %s\n",__LINE__,#value); return 1; } } while(0)
static xls_text* text(const char* source) { return xlsTextCreate(source,strlen(source),2u*1024u*1024u,NULL); }
static enum xls_query_state waitResult(xls_query* query,xls_analysis_snapshot* snapshot,xls_session_stats* stats,uint64_t* thread)
{
    uint64_t until=xrtClock()+15000000; enum xls_query_state state;
    do { state=xlsQueryResult(query,snapshot,stats,thread); if(state!=XLS_QUERY_PENDING) return state; xrtSleepUs(1000); } while(xrtClock()<until);
    return XLS_QUERY_PENDING;
}
int main(void)
{
    xls_session_limits limits={3,4096,8192}; xls_worker* worker=xlsWorkerCreate(&limits,8);
    xls_text *a=text("func alpha() as int\n return 1\nend\n"),*b=text("func beta() as int\n return 7\nend\n"),*changed=text("func alpha() as int\n return 2\nend\n");
    CHECK(worker && a && b && changed);
    char key[]="a",path[]="/memory/alpha.xl";
    xls_analysis_document inputs[]={{key,path,a,10,1},{"b","/memory/依赖😀.xl",b,20,1}};
    xls_query* query=xlsWorkerSyntax(worker,inputs,2,"a"); CHECK(query);
    key[0]='z'; path[1]='X';
    xls_analysis_snapshot first={0},second={0},result={0}; xls_session_stats stats={0}; uint64_t thread=0;
    CHECK(waitResult(query,&first,&stats,&thread)==XLS_QUERY_READY && xlsSyntaxAccepted(first.syntax) && !strcmp(xlsSyntaxPath(first.syntax),"/memory/alpha.xl") && first.identity==10 && first.revision==1);
    CHECK(thread && thread!=xrtThreadCurrentId() && stats.parses==1 && stats.documents==2); xlsQueryDestroy(query);
    key[0]='a'; path[1]='m';
    query=xlsWorkerSyntax(worker,inputs,2,"a"); CHECK(query && waitResult(query,&result,&stats,&thread)==XLS_QUERY_READY && result.syntax==first.syntax && stats.parses==1 && stats.cacheHits==1); xlsSyntaxRelease(result.syntax); xlsQueryDestroy(query);
    query=xlsWorkerSyntax(worker,inputs,2,"b"); CHECK(query && waitResult(query,&second,&stats,&thread)==XLS_QUERY_READY && stats.parses==2); xlsQueryDestroy(query);
    inputs[0].text=changed; inputs[0].revision=2;
    query=xlsWorkerSyntax(worker,inputs,2,"b"); CHECK(query && waitResult(query,&result,&stats,&thread)==XLS_QUERY_READY && result.syntax==second.syntax && stats.parses==2); xlsSyntaxRelease(result.syntax); xlsQueryDestroy(query);
    query=xlsWorkerSyntax(worker,inputs,2,"a"); CHECK(query && waitResult(query,&result,&stats,&thread)==XLS_QUERY_READY && result.syntax!=first.syntax && result.revision==2 && stats.parses==3);
    CHECK(!strcmp(xlsTextData(xlsSyntaxText(result.syntax)),xlsTextData(changed)) && !strcmp(xlsTextData(xlsSyntaxText(first.syntax)),xlsTextData(a))); xlsSyntaxRelease(result.syntax); xlsQueryDestroy(query);
    query=xlsWorkerSyntax(worker,&inputs[1],1,"b"); CHECK(query && waitResult(query,&result,&stats,&thread)==XLS_QUERY_READY && result.syntax==second.syntax && stats.documents==1 && stats.parses==3); xlsSyntaxRelease(result.syntax); xlsQueryDestroy(query);
    inputs[0].identity=30; inputs[0].revision=1; inputs[1].revision=2;
    query=xlsWorkerSyntax(worker,inputs,2,"a"); CHECK(query && waitResult(query,&result,&stats,&thread)==XLS_QUERY_READY && result.identity==30 && result.revision==1 && stats.parses==4); xlsSyntaxRelease(result.syntax); xlsQueryDestroy(query);
    query=xlsWorkerSyntax(worker,inputs,2,"b"); CHECK(query && waitResult(query,&result,&stats,&thread)==XLS_QUERY_READY && result.syntax==second.syntax && stats.parses==4); xlsSyntaxRelease(result.syntax); xlsQueryDestroy(query);
    CHECK(!xlsWorkerSyntax(worker,inputs,2,"missing"));
    xls_analysis_document duplicate[]={inputs[0],inputs[0]}; CHECK(!xlsWorkerSyntax(worker,duplicate,2,"a"));
    xls_analysis_document invalid=inputs[0]; invalid.path="\xff"; CHECK(!xlsWorkerSyntax(worker,&invalid,1,"a"));
    for(unsigned i=0;i<100;++i) {
        query=xlsWorkerSyntax(worker,inputs,2,(i&1) ? "a" : "b"); CHECK(query && waitResult(query,&result,&stats,&thread)==XLS_QUERY_READY && stats.parses==4 && stats.documents==2); xlsSyntaxRelease(result.syntax); xlsQueryDestroy(query);
    }
    xlsWorkerDestroy(worker); CHECK(xlsSyntaxAccepted(first.syntax) && xlsSyntaxAccepted(second.syntax));
    xlsSyntaxRelease(first.syntax); xlsSyntaxRelease(second.syntax); xlsTextRelease(a); xlsTextRelease(b); xlsTextRelease(changed);

    /* A running real parse must not block submission or cancellation. A single
     * queue slot enforces overload rejection without consuming caller input. */
    limits=(xls_session_limits){2,2u*1024u*1024u,4u*1024u*1024u}; worker=xlsWorkerCreate(&limits,1); CHECK(worker);
    xstrbuf large; xrtStrBufInit(&large);
    for(unsigned i=0;i<20000;++i) { char line[128]; int size=snprintf(line,sizeof(line),"func filler%u() as int\n return %u\nend\n",i,i); CHECK(size>0 && xrtStrBufAppend(&large,(xstrview){line,(size_t)size})); }
    a=text(large.Data); CHECK(a); xrtStrBufFree(&large);
    xls_analysis_document heavy={"large","/memory/large.xl",a,1,1}; query=xlsWorkerSyntax(worker,&heavy,1,"large"); CHECK(query);
    xls_worker_load load={0}; uint64_t until=xrtClock()+1000000;
    do { CHECK(xlsWorkerLoad(worker,&load)); if(load.running) break; xrtSleepUs(1000); } while(xrtClock()<until);
    CHECK(load.running==1);
    xls_query* queued=xlsWorkerSyntax(worker,&heavy,1,"large"); CHECK(queued);
    xls_query* rejected=xlsWorkerSyntax(worker,&heavy,1,"large"); CHECK(!rejected);
    xlsQueryCancel(queued);
    CHECK(waitResult(query,&result,&stats,&thread)==XLS_QUERY_READY && xlsSyntaxAccepted(result.syntax)); xlsQueryDestroy(query);
    CHECK(waitResult(queued,&first,&stats,&thread)==XLS_QUERY_CANCELLED && !first.syntax); xlsQueryDestroy(queued);
    until=xrtClock()+1000000;
    do { CHECK(xlsWorkerLoad(worker,&load)); if(load.completed==2) break; xrtSleepUs(1000); } while(xrtClock()<until);
    CHECK(load.completed==2 && load.rejected==1 && load.cancelled==1);
    xlsWorkerDestroy(worker); CHECK(xlsSyntaxAccepted(result.syntax)); xlsSyntaxRelease(result.syntax); xlsTextRelease(a);
    printf("analysis_worker_test passed: %u checks; XRT task-pool isolation, retained input/result ownership, actual parse reuse, live-set pruning, independent revisions, bounded queue and cooperative cancellation.\n",checks);
    return 0;
}
