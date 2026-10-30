#include "analysis/worker.h"
#include "analysis_test_clock.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;static size_t faultAt,faultCalls;static int faults;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_selection_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
void* __real_xrtCalloc(size_t,size_t);
bool __real_xrtArrayInit(xarray*,size_t);
bool __real_xrtArrayPush(xarray*,const void*);
static int fail(void){return faults && ++faultCalls==faultAt;}
void* __wrap_xrtCalloc(size_t count,size_t bytes){return fail() ? NULL : __real_xrtCalloc(count,bytes);}
bool __wrap_xrtArrayInit(xarray* array,size_t bytes){return fail() ? false : __real_xrtArrayInit(array,bytes);}
bool __wrap_xrtArrayPush(xarray* array,const void* value){return fail() ? false : __real_xrtArrayPush(array,value);}
static xls_text* text(const char* value){return xlsTextCreate(value,strlen(value),4u*1024u*1024u,NULL);}
static int properties(const xls_selection* result,const xls_text* source,const size_t* offsets,size_t count)
{
    CHECK(result && xlsSelectionCount(result)==count);
    for(size_t i=0;i<count;++i){size_t levels=xlsSelectionDepth(result,i);CHECK(levels && levels<=XLS_SELECTION_DEPTH);
        xls_selection_range previous={0};
        for(size_t j=0;j<levels;++j){xls_selection_range r;xls_position a,b;CHECK(xlsSelectionGet(result,i,j,&r));
            CHECK(r.start<=offsets[i] && r.end>=offsets[i] && r.end<=xlsTextSize(source));
            CHECK(xlsTextPosition(source,r.start,&a) && xlsTextPosition(source,r.end,&b));
            CHECK(a.line==r.startPosition.line && a.character==r.startPosition.character && b.line==r.endPosition.line && b.character==r.endPosition.character);
            if(j)CHECK(r.start<=previous.start && r.end>=previous.end && (r.start!=previous.start || r.end!=previous.end));
            previous=r;
        }
        CHECK(!previous.start && previous.end==xlsTextSize(source));
    }
    return 1;
}
static int equal(const xls_selection* a,const xls_selection* b)
{
    CHECK(xlsSelectionCount(a)==xlsSelectionCount(b));
    for(size_t i=0;i<xlsSelectionCount(a);++i){CHECK(xlsSelectionDepth(a,i)==xlsSelectionDepth(b,i));
        for(size_t j=0;j<xlsSelectionDepth(a,i);++j){xls_selection_range x,y;CHECK(xlsSelectionGet(a,i,j,&x) && xlsSelectionGet(b,i,j,&y));CHECK(!memcmp(&x,&y,sizeof(x)));}
    }
    return 1;
}
typedef struct cancel_state {size_t calls,at;} cancel_state;
static int cancel(void* user){cancel_state* state=user;return ++state->calls==state->at;}
static int fixture(const char* body)
{
    xls_session* session=xlsSessionCreate(NULL);xls_text* source=text(body);xls_analysis_snapshot snapshot={0};CHECK(session && source);
    CHECK(xlsSessionUpdate(session,"entry","/memory/selection.xl",1,1,source) && xlsSessionSyntax(session,"entry",&snapshot) && xlsSyntaxAccepted(snapshot.syntax));
    size_t offsets[XLS_SELECTION_POSITIONS],count=0;
    for(size_t i=0;i<=xlsTextSize(source) && count<XLS_SELECTION_POSITIONS;++i){xls_position position;if(xlsTextPosition(source,i,&position))offsets[count++]=i;}
    xls_selection* result=xlsSelectionCreate(snapshot.syntax,offsets,count,NULL,NULL);CHECK(properties(result,source,offsets,count));
    const char* number=strstr(body,"27");CHECK(number);size_t offset=(size_t)(number-body),positions[]={offset+1,xlsTextSize(source),offset+1,0};
    xls_selection* batch=xlsSelectionCreate(snapshot.syntax,positions,4,NULL,NULL);CHECK(properties(batch,source,positions,4));
    xls_selection_range r;CHECK(xlsSelectionGet(batch,0,0,&r) && r.start==offset && r.end==offset+2);
    CHECK(xlsSelectionDepth(batch,0)==xlsSelectionDepth(batch,2));
    for(size_t j=0;j<xlsSelectionDepth(batch,0);++j){xls_selection_range a,b;CHECK(xlsSelectionGet(batch,0,j,&a) && xlsSelectionGet(batch,2,j,&b) && !memcmp(&a,&b,sizeof(a)));}
    xls_selection* empty=xlsSelectionCreate(snapshot.syntax,NULL,0,NULL,NULL);CHECK(empty && !xlsSelectionCount(empty));xlsSelectionDestroy(empty);
    CHECK(!xlsSelectionCreate(NULL,positions,4,NULL,NULL) && !xlsSelectionCreate(snapshot.syntax,NULL,1,NULL,NULL));
    CHECK(!xlsSelectionCreate(snapshot.syntax,positions,XLS_SELECTION_POSITIONS+1,NULL,NULL));
    size_t bad=xlsTextSize(source)+1;CHECK(!xlsSelectionCreate(snapshot.syntax,&bad,1,NULL,NULL));
    for(size_t i=0;i<xlsTextSize(source);++i){xls_position position;if(!xlsTextPosition(source,i,&position))CHECK(!xlsSelectionCreate(snapshot.syntax,&i,1,NULL,NULL));}
    memset(&r,0xff,sizeof(r));CHECK(!xlsSelectionGet(batch,4,0,&r) && !r.start && !r.end);
    CHECK(!xlsSelectionGet(batch,0,999,&r) && !xlsSelectionGet(batch,0,0,NULL));
    cancel_state state={0,SIZE_MAX};xls_selection* probe=xlsSelectionCreate(snapshot.syntax,positions,4,cancel,&state);CHECK(probe);xlsSelectionDestroy(probe);
    for(size_t i=1;i<=state.calls;++i){cancel_state stop={0,i};CHECK(!xlsSelectionCreate(snapshot.syntax,positions,4,cancel,&stop));}
    faults=1;faultAt=SIZE_MAX;faultCalls=0;probe=xlsSelectionCreate(snapshot.syntax,positions,4,NULL,NULL);faults=0;CHECK(probe);size_t sites=faultCalls;xlsSelectionDestroy(probe);
    for(size_t i=1;i<=sites;++i){faults=1;faultAt=i;faultCalls=0;probe=xlsSelectionCreate(snapshot.syntax,positions,4,NULL,NULL);faults=0;CHECK(!probe);}
    /* Published ranges survive all producing owners; current units must still
     * agree with an independent fresh parse after coordinate-moving edits. */
    char changed[4096];int bom=!memcmp(body,"\xef\xbb\xbf",3);snprintf(changed,sizeof(changed),"%s// shifted 😀\r\n%s",bom ? "\xef\xbb\xbf" : "",body+(bom ? 3 : 0));xls_text* update=text(changed);CHECK(update && xlsSessionUpdate(session,"entry","/memory/selection.xl",1,2,update));
    xls_analysis_snapshot updated={0},fresh={0};CHECK(xlsSessionSyntax(session,"entry",&updated) && xlsSyntaxAccepted(updated.syntax));
    xls_session* separate=xlsSessionCreate(NULL);CHECK(separate && xlsSessionUpdate(separate,"entry","/memory/selection.xl",2,1,update) && xlsSessionSyntax(separate,"entry",&fresh));
    size_t shifted=(size_t)(strstr(changed,"27")-changed)+1;
    xls_selection *incremental=xlsSelectionCreate(updated.syntax,&shifted,1,NULL,NULL),*reference=xlsSelectionCreate(fresh.syntax,&shifted,1,NULL,NULL);CHECK(incremental && reference && equal(incremental,reference));
    xlsSyntaxRelease(fresh.syntax);xlsSyntaxRelease(updated.syntax);xlsSessionDestroy(separate);xlsTextRelease(update);
    xlsSyntaxRelease(snapshot.syntax);xlsSessionDestroy(session);CHECK(properties(result,source,offsets,count) && properties(batch,source,positions,4));
    xlsSelectionDestroy(reference);xlsSelectionDestroy(incremental);xlsSelectionDestroy(batch);xlsSelectionDestroy(result);xlsTextRelease(source);return 1;
}
static int errors(void)
{
    const char* bodies[]={"","func broken(","// only comment 😀\r\n"};
    for(size_t i=0;i<3;++i){xls_session* session=xlsSessionCreate(NULL);xls_text* source=text(bodies[i]);xls_analysis_snapshot snapshot={0};CHECK(session && source && xlsSessionUpdate(session,"e","/memory/error.xl",1,1,source) && xlsSessionSyntax(session,"e",&snapshot));
        size_t positions[]={0,xlsTextSize(source)};xls_selection* result=xlsSelectionCreate(snapshot.syntax,positions,2,NULL,NULL);CHECK(properties(result,source,positions,2));
        if(!xlsSyntaxAccepted(snapshot.syntax)){xls_selection_range r;CHECK(xlsSelectionGet(result,1,0,&r) && r.start==positions[1] && r.end==positions[1]);}
        xlsSelectionDestroy(result);xlsSyntaxRelease(snapshot.syntax);xlsSessionDestroy(session);xlsTextRelease(source);
    }
    return 1;
}
static enum xls_query_state wait(xls_query* query,xls_analysis_snapshot* result,xls_session_stats* stats)
{
    uint64_t until=xlsTestClockMicros()+15000000,thread=0;enum xls_query_state state;
    do{state=xlsQueryResult(query,result,stats,&thread);if(state!=XLS_QUERY_PENDING)return state;xlsTestSleepMicros(1000);}while(xlsTestClockMicros()<until);return XLS_QUERY_PENDING;
}
static int worker(void)
{
    const char* body="func main(...) as int\n return 27\nend\n";xls_text* source=text(body);xls_worker* actor=xlsWorkerCreate(NULL,8);CHECK(source && actor);
    xls_analysis_document input={"entry","/memory/worker.xl",source,1,1};size_t offset=(size_t)(strstr(body,"27")-body)+1,saved=offset;
    xls_query* query=xlsWorkerSelection(actor,&input,1,"entry",&offset,1);CHECK(query);offset=0;
    xls_analysis_snapshot snapshot={0};xls_session_stats stats={0};CHECK(wait(query,&snapshot,&stats)==XLS_QUERY_READY && stats.parses==1);
    const xls_selection* result=xlsQuerySelection(query);CHECK(properties(result,source,&saved,1));xls_selection_range r;CHECK(xlsSelectionGet(result,0,0,&r) && r.start==saved-1 && r.end==saved+1);xlsSyntaxRelease(snapshot.syntax);xlsQueryDestroy(query);
    query=xlsWorkerSelection(actor,&input,1,"entry",&saved,1);CHECK(query && wait(query,&snapshot,&stats)==XLS_QUERY_READY && stats.parses==1 && stats.cacheHits);xlsSyntaxRelease(snapshot.syntax);xlsQueryDestroy(query);
    CHECK(!xlsWorkerSelection(actor,&input,1,"entry",NULL,1));saved=xlsTextSize(source)+1;CHECK(!xlsWorkerSelection(actor,&input,1,"entry",&saved,1));
    query=xlsWorkerSelection(actor,&input,1,"entry",NULL,0);CHECK(query);xlsQueryCancel(query);enum xls_query_state state=wait(query,&snapshot,&stats);CHECK(state==XLS_QUERY_CANCELLED || state==XLS_QUERY_READY);xlsSyntaxRelease(snapshot.syntax);xlsQueryDestroy(query);
    xlsWorkerDestroy(actor);xlsTextRelease(source);return 1;
}
int main(void)
{
    if(!fixture("func main(...) as int\n int value = 27\n return value+1\nend\n") ||
       !fixture("\xef\xbb\xbf// 中文😀\r\nclass Box\r\n func value() as int\r\n  return 27\r\n end\r\nend\r\n") || !errors() || !worker())return 1;
    printf("analysis_selection_test passed: %u checks; original literal bounds,nested chains,UTF16/BOM/CRLF,incremental/fresh parity,owned lifetime,batched worker,all observed cancellation and allocation boundaries\n",checks);return 0;
}
