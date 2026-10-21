#include "analysis/worker.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if(!(value)) { fprintf(stderr,"analysis_folding_test:%d: %s\n",__LINE__,#value); return 1; } } while(0)
static int cancel(void* user) { unsigned* visits=user; return ++*visits>=2; }
static void wake(void* user) { xrtAtomic32FetchAdd(user,1,XMEMORY_RELAXED); }
static enum xls_query_state wait(xls_query* query,xls_analysis_snapshot* result,xls_session_stats* stats)
{
    uint64_t until=xrtClock()+10000000; enum xls_query_state state;
    do { state=xlsQueryResult(query,result,stats,NULL); if(state!=XLS_QUERY_PENDING) return state; xrtSleepUs(1000); } while(xrtClock()<until);
    return state;
}
int main(void)
{
    const char* source="class Box\n int value\n func getValue() as int\n  return self.value\n end\nend\nfunc main(...) as int\n if true\n  return 1\n else\n  return 0\n end\nend\n";
    const xls_fold_range expected[]={{0,5},{2,4},{6,12},{7,11}};
    xls_text* text=xlsTextCreate(source,strlen(source),4096,NULL); xls_worker* worker=xlsWorkerCreate(NULL,8);
    CHECK(text && worker);
    xls_analysis_document input={"source","/memory/折叠😀.xl",text,1,1};
    xls_query* query=xlsWorkerFolding(worker,&input,1,input.key,100); xls_analysis_snapshot snapshot={0}; xls_session_stats stats={0};
    xatomic32 wakes; xrtAtomic32Init(&wakes,0); CHECK(query && xlsQuerySetWake(query,wake,&wakes) && !xlsQuerySetWake(query,wake,&wakes));
    CHECK(query && wait(query,&snapshot,&stats)==XLS_QUERY_READY && xlsSyntaxAccepted(snapshot.syntax) && stats.parses==1);
    const xls_folding* folding=xlsQueryFolding(query); const xls_fold_range* ranges=xlsFoldingRanges(folding);
    CHECK(folding && xlsFoldingCount(folding)==4);
    for(size_t i=0;i<4;++i) CHECK(ranges[i].startLine==expected[i].startLine && ranges[i].endLine==expected[i].endLine);
    unsigned visits=0; CHECK(!xlsFoldingCreate(snapshot.syntax,100,cancel,&visits));
    xls_folding* empty=xlsFoldingCreate(snapshot.syntax,0,NULL,NULL); CHECK(empty && !xlsFoldingCount(empty)); xlsFoldingDestroy(empty);
    xlsSyntaxRelease(snapshot.syntax); xlsQueryDestroy(query); CHECK(xrtAtomic32Load(&wakes,XMEMORY_RELAXED)==1);
    query=xlsWorkerFolding(worker,&input,1,input.key,2); CHECK(query && wait(query,&snapshot,&stats)==XLS_QUERY_READY && stats.parses==1 && xlsFoldingCount(xlsQueryFolding(query))==2); xlsSyntaxRelease(snapshot.syntax); xlsQueryDestroy(query);
    char crlf[4096]; size_t at=0; memcpy(crlf,"\xef\xbb\xbf",3); at=3;
    for(const char* p=source;*p;++p) { if(*p=='\n') crlf[at++]='\r'; crlf[at++]=*p; } crlf[at]=0;
    xls_text* replaced=xlsTextCreate(crlf,at,4096,NULL); CHECK(replaced); input.text=replaced; input.revision=2;
    query=xlsWorkerFolding(worker,&input,1,input.key,100); CHECK(query && wait(query,&snapshot,&stats)==XLS_QUERY_READY && stats.parses==2 && xlsFoldingCount(xlsQueryFolding(query))==4);
    ranges=xlsFoldingRanges(xlsQueryFolding(query)); for(size_t i=0;i<4;++i) CHECK(ranges[i].startLine==expected[i].startLine && ranges[i].endLine==expected[i].endLine); xlsSyntaxRelease(snapshot.syntax); xlsQueryDestroy(query); xlsTextRelease(replaced);
    const char* broken="func broken(\n"; replaced=xlsTextCreate(broken,strlen(broken),4096,NULL); CHECK(replaced); input.text=replaced; input.revision=3;
    query=xlsWorkerFolding(worker,&input,1,input.key,100); CHECK(query && wait(query,&snapshot,&stats)==XLS_QUERY_READY && !xlsSyntaxAccepted(snapshot.syntax) && xlsFoldingCount(xlsQueryFolding(query))==0);
    xlsSyntaxRelease(snapshot.syntax); xlsQueryDestroy(query); xlsTextRelease(replaced); xlsTextRelease(text); xlsWorkerDestroy(worker);
    printf("analysis_folding_test passed: %u checks; shared AST structural ranges, nested scopes, stable ordering, output bounds, BOM/CRLF, invalid syntax, cancellation and cached worker queries.\n",checks);
    return 0;
}
