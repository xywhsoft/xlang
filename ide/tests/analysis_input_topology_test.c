#include "analysis/input_topology.h"
#include "analysis/frontend_input_usage.h"
#include "src/compile/xl6_source_overlay.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define main originalWorkerFixtureMain
#include "analysis_worker_cache_test.c"
#undef main
#pragma GCC diagnostic pop
static xls_input_topology* capture(XL6_SourceOverlay* overlay,const char* hit,const char* miss,size_t bytes)
{
    xls_input_usage_scope usage;xlsFrontendInputUsageBegin(&usage);
    xls_input_topology_scope scope;xlsInputTopologyBegin(&scope,bytes);xlsFrontendInputUsageBind(overlay,overlay->count);
    (void)xl6SourceOverlayFind(overlay,hit);(void)xl6SourceOverlayFind(overlay,miss);
    uint64_t used=0;int known=xlsFrontendInputUsageEnd(&usage,&used);
    xls_input_topology* topology=xlsInputTopologyEnd(&scope);
    if(!known || used!=1){xlsInputTopologyDestroy(topology);return NULL;}return topology;
}
typedef struct topology_reader {const xls_input_topology* topology;const char *extra,*hit,*miss;int okay;} topology_reader;
static int32 topologyRead(void* user)
{
    topology_reader* r=user;r->okay=1;const char* current[]={r->hit,r->extra};
    for(unsigned i=0;i<64;++i){
        if(!xlsInputTopologyAdmits(r->topology,&r->extra,1,current,2) || xlsInputTopologyAdmits(r->topology,&r->miss,1,current,2)){r->okay=0;break;}
    }return 0;
}
static int admits(const xls_input_topology* topology,const char* path,const char* const* current,size_t count)
{return xlsInputTopologyAdmits(topology,&path,1,current,count);}
static int observations(void)
{
    char *hit=xrtPathAbs("topology-Ä.xl"),*miss=xrtPathAbs("missing.xl"),*extra=xrtPathAbs("unrelated.xl");CHECK(hit && miss && extra);
    XL6_SourceOverlay overlay={0};CHECK(xl6SourceOverlaySet(&overlay,hit,"",0));
    xls_input_topology* topology=capture(&overlay,hit,miss,32768);CHECK(topology);xls_input_topology_info info=xlsInputTopologyInfo(topology);CHECK(info.paths==2 && info.bytes>0 && info.bytes<=32768);
    const char* current[]={hit,extra};CHECK(admits(topology,extra,current,2));CHECK(!admits(topology,miss,current,2));CHECK(!admits(topology,hit,current,2));
    const char* duplicate[]={hit,hit};CHECK(!admits(topology,extra,duplicate,2));
    char* alternate=xrtStrDup(hit);CHECK(alternate);for(char* p=alternate;*p;++p)if(*p=='\\')*p='/';
    CHECK(!admits(topology,alternate,current,2));
#ifdef _WIN32
    char* unicode=strstr(alternate,"Ä");CHECK(unicode);memcpy(unicode,"ä",2);CHECK(!admits(topology,alternate,current,2));
    const char* folded[]={hit,alternate};CHECK(!admits(topology,extra,folded,2));
#endif
    topology_reader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(topology_reader){topology,extra,hit,miss,0};threads[i]=xrtThreadCreate(topologyRead,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
    for(size_t bytes=0;bytes<256;++bytes){xls_input_topology* small=capture(&overlay,hit,miss,bytes);if(small){CHECK(xlsInputTopologyInfo(small).bytes<=bytes && !admits(small,miss,current,2));}xlsInputTopologyDestroy(small);}
    xls_input_topology_scope outer,inner;xlsInputTopologyBegin(&outer,32768);xlsInputTopologyBind(&overlay,1);xlsInputTopologyLookup(&overlay,hit);
    xlsInputTopologyBegin(&inner,0);xlsInputTopologyLookup(&overlay,miss);CHECK(!xlsInputTopologyEnd(&inner));xlsInputTopologyLookup(&overlay,extra);
    xls_input_topology* nested=xlsInputTopologyEnd(&outer);CHECK(nested && xlsInputTopologyInfo(nested).paths==2);xlsInputTopologyDestroy(nested);
    xlsInputTopologyBegin(&outer,32768);xlsInputTopologyBind(&overlay,1);xlsInputTopologyLookup(&overlay,hit);xlsInputTopologyFailure(NULL);CHECK(!xlsInputTopologyEnd(&outer));
    xl6SourceOverlayClear(&overlay);CHECK(admits(topology,extra,current,2));xlsInputTopologyDestroy(topology);xrtFree(hit);xrtFree(miss);xrtFree(extra);xrtFree(alternate);return 1;
}
static int workerMembership(void)
{
    char relative[160];snprintf(relative,sizeof(relative),"build/topology-workers-%llu",(unsigned long long)xrtClock());char* root=xrtPathAbs(relative);CHECK(root && xrtDirCreateAll(root));
    char* paths[64];xls_analysis_document docs[64];memset(docs,0,sizeof(docs));
    const char* source="\xef\xbb\xbfimport dep\r\nfunc choose(int left,string right) as int\r\n return left\r\nend\r\nfunc stable(int value) as int\r\n if value>0 then\r\n return value\r\n end\r\n return 0\r\nend\r\nfunc main(...) as int\r\n int unique_main=choose(dep.compute(),\"hi\")\r\n return unique_main\r\nend\r\n";
    const char* dependency="export func compute() as int\r\n return 7\r\nend\r\n";
    for(size_t i=0;i<64;++i){char file[64],key[64];snprintf(file,sizeof(file),i==0 ? "main.xl" : i==1 ? "dep.xl" : "unrelated-%zu.xl",i);snprintf(key,sizeof(key),i==0 ? "main" : i==1 ? "dep" : "extra-%zu",i);paths[i]=xrtPathJoin(root,file);docs[i]=(xls_analysis_document){xrtStrDup(key),paths[i],xlsTextCreate(i==0 ? source : i==1 ? dependency : "// 中文😀\r\nfunc unfinished(",i==0 ? strlen(source) : i==1 ? strlen(dependency) : strlen("// 中文😀\r\nfunc unfinished("),1024*1024,NULL),i+1,1};CHECK(docs[i].key && paths[i] && docs[i].text);}
    xls_worker* workers[3];fixture_result held[3]={0};
    for(unsigned k=0;k<3;++k){workers[k]=xlsWorkerCreate(NULL,4);CHECK(workers[k]);held[k]=query(workers[k],k,NULL,docs,2,"main","main");CHECK(held[k].state==XLS_QUERY_READY && parity(&held[k],k,NULL,docs,2,"main","main"));}
    const size_t counts[]={3,8,32,64,17,2,63,2};
    for(size_t n=0;n<sizeof(counts)/sizeof(*counts);++n){
        size_t count=counts[n];xls_analysis_document ordered[64];for(size_t i=0;i<count;++i)ordered[i]=docs[count-i-1];
        for(unsigned k=0;k<3;++k){fixture_result r=query(workers[k],k,NULL,ordered,count,"main","main");CHECK(r.state==XLS_QUERY_READY && builds(&r,k)==1 && hits(&r,k)==n+1 && sameResult(&r,&held[k],k) && parity(&r,k,NULL,ordered,count,"main","main"));releaseResult(&r);}
    }
    ++docs[2].identity;docs[2].revision=1;
    for(unsigned k=0;k<3;++k){fixture_result r=query(workers[k],k,NULL,docs,3,"main","main");CHECK(r.state==XLS_QUERY_READY && sameResult(&r,&held[k],k) && parity(&r,k,NULL,docs,3,"main","main"));releaseResult(&r);}
    xls_analysis_document duplicate[3]={docs[0],docs[1],docs[2]};duplicate[2].path=docs[0].path;
    for(unsigned k=0;k<3;++k){fixture_result r=query(workers[k],k,NULL,duplicate,3,"main","main");CHECK(r.state==XLS_QUERY_FAILED);releaseResult(&r);}
    const char* edited="export func compute() as int\r\n return 8\r\nend\r\n";xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate(edited,strlen(edited),1024*1024,NULL);++docs[1].revision;CHECK(docs[1].text);
    for(unsigned k=0;k<3;++k){fixture_result r=query(workers[k],k,NULL,docs,3,"main","main");CHECK(r.state==XLS_QUERY_READY && parity(&r,k,NULL,docs,3,"main","main"));CHECK(k==CHECK_KIND ? xlsSemanticCheckWork(r.check).functionsReused>=1 : k==COMPLETION_KIND ? xlsSemanticCompletionSemWork(r.completion).reused>=1 : xlsSignatureWork(r.signature).reused>=1);releaseResult(&r);}
    xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate(dependency,strlen(dependency),1024*1024,NULL);++docs[1].revision;CHECK(docs[1].text);
    ++docs[1].identity;docs[1].revision=1;
    for(unsigned k=0;k<3;++k){fixture_result r=query(workers[k],k,NULL,docs,3,"main","main");CHECK(r.state==XLS_QUERY_READY && !sameResult(&r,&held[k],k) && parity(&r,k,NULL,docs,3,"main","main"));releaseResult(&r);xlsWorkerDestroy(workers[k]);CHECK(parity(&held[k],k,NULL,docs,2,"main","main"));releaseResult(&held[k]);}
    for(size_t i=0;i<64;++i){xrtFree((void*)docs[i].key);xrtFree(paths[i]);xlsTextRelease(docs[i].text);}xrtFree(root);return 1;
}
int main(void)
{checks=0;if(!observations() || !workerMembership())return 1;printf("analysis_input_topology_test passed: %u checks;actual hit/miss paths,Unicode original matcher,small budgets,nested scopes,four readers,64 reordered overlays,three retained native models and duplicate/observed identity rejection with complete fresh parity\n",checks);return 0;}
