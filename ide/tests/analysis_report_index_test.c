#include <xrt.h>
#include "analysis/frontend_context.h"
#include "analysis/input_topology.h"
#include "src/common/xl6_path_identity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static _Thread_local size_t fault=SIZE_MAX,calls,keyFault=SIZE_MAX,keys,compareFault=SIZE_MAX,comparisons;
static void* indexAllocate(size_t a,size_t b){return calls++==fault?NULL:xrtCalloc(a,b);}
static xls_input_path_key indexKey(const char* p,int absolute){return keys++==keyFault?(xls_input_path_key){0}:xlsInputPathPrepare(p,absolute);}
static int indexCompare(const xls_input_path_key* a,const xls_input_path_key* b,int* okay)
{if(comparisons++==compareFault){*okay=0;return 0;}return xlsInputPathCompare(a,b,okay);}
#define XLS_REPORT_INDEX_ALLOC indexAllocate
#define xlsInputPathPrepare indexKey
#define xlsInputPathCompare indexCompare
#include "../src/analysis/report_index.c"
#undef xlsInputPathPrepare
#undef xlsInputPathCompare
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_report_index_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static uint64_t blocks(void){xmemstats s;xrtMemStatsGet(&s);return s.BlockAllocCalls-s.BlockFreeCalls;}
static size_t original(const XL6_SourceReport* r,const char* path,enum xls_report_index_kind kind)
{
 size_t count=kind==XLS_REPORT_SOURCE?r->count:r->probeCount;
 for(size_t i=0;i<count;++i){const char* p=kind==XLS_REPORT_SOURCE?r->items[i].path:r->probes[i].path;if(xl6PathIdentityEqual(p,path)==1)return i;}return SIZE_MAX;
}
static int lookup(XL6_SourceReport* r,const char* path,enum xls_report_index_kind kind)
{
 size_t index=17;int result=xlsReportIndexLookup(r,path,kind,&index);size_t expected=original(r,path,kind);
 CHECK(result==0 || result==1 || result==2);if(result!=2)CHECK(index==expected && result==(expected!=SIZE_MAX));else CHECK(index==SIZE_MAX);
 return 1;
}
static int model(void)
{
 const size_t count=384;char (*paths)[160]=xrtCalloc(count,sizeof(*paths));XL6_SourceReportItem* sources=xrtCalloc(count,sizeof(*sources));XL6_ResolutionProbe* probes=xrtCalloc(count,sizeof(*probes));CHECK(paths && sources && probes);
 for(size_t i=0;i<count;++i){snprintf(paths[i],160,"D:/same-É-😀/class-%04zu.xl",i);sources[i].path=paths[(i*73)%count];probes[i]=(XL6_ResolutionProbe){paths[(i*151)%count],(int)(i&1)};}
 XL6_SourceReport report={.items=sources,.count=count,.probes=probes,.probeCount=count,.complete=1};xls_frontend_context_scope scope;uint64_t live=blocks();
 CHECK(lookup(&report,paths[0],XLS_REPORT_SOURCE) && !xlsFrontendReportIndexSlot());
 XL6_SourceReport tiny=report;tiny.count=8;tiny.probeCount=49;
 xlsFrontendContextBegin(&scope,NULL);CHECK(lookup(&tiny,paths[3],XLS_REPORT_SOURCE) && lookup(&tiny,paths[3],XLS_REPORT_PROBE) && !scope.reportIndexes);CHECK(xlsFrontendContextEnd(&scope) && blocks()==live);
 xlsFrontendContextBegin(&scope,NULL);calls=0;keys=0;comparisons=0;
 for(size_t i=0;i<count;++i)CHECK(lookup(&report,paths[i],XLS_REPORT_SOURCE) && lookup(&report,paths[i],XLS_REPORT_PROBE));
 size_t allocated=calls,prepared=keys,compared=comparisons;CHECK(allocated && prepared && compared);xls_report_index_work work;xlsReportIndexesWork(scope.reportIndexes,&work);CHECK(work.preparedRows==2*count && work.indexed==2*count && !work.declines && work.comparisons<100000);
 for(size_t i=0;i<5000;++i)CHECK(lookup(&report,paths[(i*79)%count],(i&1)?XLS_REPORT_SOURCE:XLS_REPORT_PROBE));
 CHECK(lookup(&report,"D:/same-é-😀/CLASS-0007.XL",XLS_REPORT_SOURCE) && lookup(&report,"D:/absent.xl",XLS_REPORT_PROBE));
 xlsReportIndexesWork(scope.reportIndexes,&work);CHECK(work.preparedRows==2*count && work.peakBytes<8u*1024u*1024u && work.comparisons<250000);
 /* Exact first-row semantics for source/probe paths, including aliases. */
 xlsFrontendReportForget(&report);sources[1].path=sources[0].path;probes[1].path=probes[0].path;CHECK(lookup(&report,sources[0].path,XLS_REPORT_SOURCE) && lookup(&report,probes[0].path,XLS_REPORT_PROBE));sources[1].path=paths[73];probes[1].path=paths[151];xlsFrontendReportForget(&report);
 CHECK(xlsFrontendContextEnd(&scope) && blocks()==live);
 /* Optional allocation/key/comparison refusal ALWAYS goes back to the exact
  * original scan; it cannot claim a miss or invalidate the native report. */
 for(size_t i=0;i<allocated;++i){fault=i;calls=0;xlsFrontendContextBegin(&scope,NULL);size_t at=0;CHECK(xlsReportIndexLookup(&report,paths[3],XLS_REPORT_SOURCE,&at)==2 && at==SIZE_MAX && report.complete);size_t before=calls;CHECK(lookup(&report,paths[7],XLS_REPORT_SOURCE) && calls==before);CHECK(xlsFrontendContextEnd(&scope) && blocks()==live);}fault=SIZE_MAX;
 for(size_t i=0;i<2*count+1;++i){keyFault=i;keys=0;xlsFrontendContextBegin(&scope,NULL);size_t at=0;CHECK(xlsReportIndexLookup(&report,paths[3],XLS_REPORT_SOURCE,&at)==2 && at==SIZE_MAX && report.complete);CHECK(xlsFrontendContextEnd(&scope) && blocks()==live);}keyFault=SIZE_MAX;
 /* The first384 lookups above observed many queries; calibrate just one
  * complete capture/query so every comparator refusal precedes publication. */
 comparisons=0;xlsFrontendContextBegin(&scope,NULL);CHECK(lookup(&report,paths[3],XLS_REPORT_SOURCE));size_t compareCount=comparisons;CHECK(xlsFrontendContextEnd(&scope) && blocks()==live);
 for(size_t i=0;i<compareCount;++i){compareFault=i;comparisons=0;xlsFrontendContextBegin(&scope,NULL);size_t at=0;CHECK(xlsReportIndexLookup(&report,paths[3],XLS_REPORT_SOURCE,&at)==2 && at==SIZE_MAX && report.complete);CHECK(xlsFrontendContextEnd(&scope) && blocks()==live);}compareFault=SIZE_MAX;
 /* Native Init/Free forget a report in outer scopes even while another
  * frontend context is active. Reusing the address must not reuse old keys. */
 xlsFrontendContextBegin(&scope,NULL);CHECK(lookup(&report,paths[3],XLS_REPORT_SOURCE));xls_frontend_context_scope nested;xlsFrontendContextBegin(&nested,NULL);xl6SourceReportInit(&report);CHECK(xlsFrontendContextEnd(&nested));report=(XL6_SourceReport){.items=sources,.count=count,.probes=probes,.probeCount=count,.complete=1};sources[0].path="D:/changed.xl";CHECK(lookup(&report,paths[0],XLS_REPORT_SOURCE) && lookup(&report,"D:/changed.xl",XLS_REPORT_SOURCE));CHECK(xlsFrontendContextEnd(&scope) && blocks()==live);
 xrtFree(probes);xrtFree(sources);xrtFree(paths);return 1;
}
static int growthAndQuota(void)
{
 const size_t count=512,width=12000;char* paths=xrtMalloc(count*width);XL6_SourceReportItem* rows=xrtCalloc(count,sizeof(*rows));CHECK(paths && rows);
 for(size_t i=0;i<count;++i){char* p=paths+i*width;memset(p,'a',width-1);snprintf(p,24,"D:/%06zu/",i);p[10]='a';p[width-1]=0;rows[i].path=p;}
 XL6_SourceReport report={.items=rows,.count=count,.complete=1};xls_frontend_context_scope scope;uint64_t live=blocks();xlsFrontendContextBegin(&scope,NULL);size_t at=0;CHECK(xlsReportIndexLookup(&report,rows[count-1].path,XLS_REPORT_SOURCE,&at)==2 && at==SIZE_MAX && report.complete);xls_report_index_work work;xlsReportIndexesWork(scope.reportIndexes,&work);CHECK(work.declines==1 && work.peakBytes<=8u*1024u*1024u);CHECK(xlsFrontendContextEnd(&scope) && blocks()==live);
 xrtFree(rows);xrtFree(paths);return 1;
}
int main(void){if(!model() || !growthAndQuota())return 1;printf("analysis_report_index_test passed: %u checks; original native path/first-row oracle, prepared-source/probe AVL lookups, all calibrated allocation/key/comparison refusals, nested Init/Free invalidation, quota fallback and balanced ownership\n",checks);return 0;}
