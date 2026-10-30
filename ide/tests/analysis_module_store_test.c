#include "analysis_test_clock.h"
#include "analysis/module_store.h"
#include "analysis/worker_modules.h"
#include "analysis/frontend_entry_cache.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define main originalWorkerFixtureMain
#include "analysis_worker_cache_test.c"
#undef main
#pragma GCC diagnostic pop
static xls_syntax* ordinary(const char* path,const char* source)
{
    XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
    xls_text* text=xlsTextCreate(source,strlen(source),1024*1024,NULL);
    int accepted=text && xl6ParseSource(source,path,&parsed);
    xls_syntax* syntax=accepted ? xlsSyntaxAdoptOrdinary(path,text,&parsed.root) : NULL;
    xlsTextRelease(text);xl6ParseResultFree(&parsed);return syntax;
}
static const char *firstSource="// 中文😀\r\nfunc value(int count) as int\r\n return count+1\r\nend\r\n",
 *nextSource="// 中文😀\r\nfunc value(int count) as int\r\n return count+2\r\nend\r\n";
static int pendingAndBudget(void)
{
    xls_module_store* store=xlsModuleStoreCreate(NULL);xls_entry_cache* cache=xlsEntryCacheCreate(1024*1024);CHECK(store && cache);
    xls_module_scope bank;xlsModuleScopeBegin(&bank,store);
    for(unsigned commit=0;commit<2;++commit){
        xls_entry_cache_scope scope;XL6_ParseResult parsed;xl6ParseResultInit(&parsed);xlsFrontendEntryBegin(&scope,cache,NULL,"entry.xl");
        CHECK(xlsFrontendEntryParse(firstSource,"dep.xl",&parsed,NULL) && parsed.root && !parsed.diags.count);
        xls_module_stats stats;xlsModuleStoreStats(store,&stats);CHECK(!stats.published);
        xlsFrontendEntryEnd(&scope,(int)commit);xl6ParseResultFree(&parsed);xlsModuleStoreStats(store,&stats);CHECK(stats.published==commit);
    }
    xls_entry_cache_scope scope;XL6_ParseResult parsed;xl6ParseResultInit(&parsed);xlsFrontendEntryBegin(&scope,cache,NULL,"entry.xl");
    CHECK(!xlsFrontendEntryParse("func broken(\n","dep.xl",&parsed,NULL) && parsed.diags.count);
    xlsFrontendEntryEnd(&scope,1);xl6ParseResultFree(&parsed);xls_module_stats stats;xlsModuleStoreStats(store,&stats);CHECK(stats.published==1);
    int exact=0;xls_module_snapshot* held=xlsModuleStoreAcquire(store,"dep.xl",firstSource,strlen(firstSource),&exact);CHECK(held && exact);xlsModuleSnapshotRelease(held);
    xlsModuleScopeEnd(&bank);xlsEntryCacheDestroy(cache);xlsModuleStoreRelease(store);
    store=xlsModuleStoreCreate(&(xls_module_limits){1,1,4096});cache=xlsEntryCacheCreate(1);CHECK(store && cache);xlsModuleScopeBegin(&bank,store);
    xl6ParseResultInit(&parsed);xlsFrontendEntryBegin(&scope,cache,NULL,"entry.xl");
    CHECK(xlsFrontendEntryParse(firstSource,"dep.xl",&parsed,NULL) && parsed.root && !parsed.diags.count);
    xlsFrontendEntryEnd(&scope,1);CHECK(scope.moduleWork.declined);xl6ParseResultFree(&parsed);xlsModuleStoreStats(store,&stats);CHECK(!stats.versions && !stats.published);
    CHECK(!xlsModuleStoreIdentity(store,"another.xl"));xlsModuleScopeEnd(&bank);xlsEntryCacheDestroy(cache);xlsModuleStoreRelease(store);return 1;
}
static char* irJson(const xls_semantic_check* result)
{
    const XL6_IrModule* ir=xlsSemanticCheckIr(result);if(!ir)return NULL;
    XL6_Output output;if(!xl6OutputOpenMemory(&output))return NULL;xl6IrDumpModuleJson(&output,ir);
    char* text=xl6OutputHasFailed(&output) ? NULL : xl6OutputTakeMemoryText(&output,NULL);xl6OutputDiscard(&output);return text;
}
static int irParity(const fixture_result* result,xls_context* context,const xls_analysis_document* docs)
{
    xls_semantic_input inputs[2];for(size_t i=0;i<2;++i)inputs[i]=(xls_semantic_input){docs[i].path,xlsTextData(docs[i].text),xlsTextSize(docs[i].text)};
    xls_semantic_check* fresh=xlsSemanticCheckCreateInContext(context,docs[0].path,inputs,2,1,NULL,NULL,NULL,0);CHECK(fresh && xlsSemanticCheckInfo(fresh).accepted);
    char *a=irJson(fresh),*b=irJson(result->check);CHECK(a && b && !strcmp(a,b));xrtFree(a);xrtFree(b);xlsSemanticCheckDestroy(fresh);return 1;
}
static int snapshots(void)
{
    CHECK(!xlsModuleStoreCreate(&(xls_module_limits){0,1,4096}));CHECK(!xlsModuleStoreCreate(&(xls_module_limits){1,0,4096}));CHECK(!xlsModuleStoreCreate(&(xls_module_limits){1,1,1}));
    xls_module_store* s=xlsModuleStoreCreate(&(xls_module_limits){8,2,1024*1024});CHECK(s);
    xls_syntax *a=ordinary("a.xl",firstSource),*b=ordinary("a.xl",nextSource),*other=ordinary("other.xl",firstSource);CHECK(a && b && other);
    CHECK(!xlsModuleStoreIdentity(s,"") && !xlsModuleStoreIdentity(s,"\xff"));uint64_t id=xlsModuleStoreIdentity(s,"a.xl");CHECK(id && xlsModuleStoreIdentity(s,"a.xl")==id);
    CHECK(xlsModuleStorePublish(s,"a.xl",a));int exact=0;xls_module_snapshot* old=xlsModuleStoreAcquire(s,"a.xl",firstSource,strlen(firstSource),&exact);CHECK(old && exact && xlsModuleSnapshotIdentity(old)==id);
    CHECK(xlsModuleStorePublish(s,"a.xl",b));xls_module_snapshot* current=xlsModuleStoreAcquire(s,"a.xl",nextSource,strlen(nextSource),&exact);CHECK(current && exact && xlsModuleSnapshotIdentity(current)==id && xlsModuleSnapshotRevision(current)!=xlsModuleSnapshotRevision(old));
    xls_module_snapshot* again=xlsModuleStoreAcquire(s,"a.xl",firstSource,strlen(firstSource),&exact);CHECK(again==old && exact);xlsModuleSnapshotRelease(again);
    CHECK(xlsModuleStorePublish(s,"other.xl",other));again=xlsModuleStoreAcquire(s,"a.xl",nextSource,strlen(nextSource),&exact);CHECK(again && !exact && xlsModuleSnapshotIdentity(again)==id);xlsModuleSnapshotRelease(again);
    CHECK(xlsModuleStoreIdentity(s,"other.xl")!=id && xlsModuleStoreIdentity(s,"a.xl")==id);
    xls_module_stats stats;xlsModuleStoreStats(s,&stats);CHECK(stats.modules==2 && stats.versions==2 && stats.evictions && stats.bytes<=stats.limit);
    CHECK(!xlsModuleStorePublish(s,"wrong.xl",a));
    xls_module_scope outer,inner;xlsModuleScopeBegin(&outer,s);CHECK(xlsModuleScopeCurrent()==s);xlsModuleScopeBegin(&inner,NULL);CHECK(!xlsModuleScopeCurrent());xlsModuleScopeEnd(&inner);CHECK(xlsModuleScopeCurrent()==s);xlsModuleScopeEnd(&outer);CHECK(!xlsModuleScopeCurrent());
    xlsModuleStoreRelease(s);CHECK(!strcmp(xlsModuleSnapshotPath(old),"a.xl") && !strcmp(xlsTextData(xlsSyntaxText(xlsModuleSnapshotSyntax(current))),nextSource));
    XL6_ParseResult copy;xl6ParseResultInit(&copy);CHECK(xlsSyntaxCopyParse(xlsModuleSnapshotSyntax(current),&copy) && copy.root);xl6ParseResultFree(&copy);
    xlsModuleSnapshotRelease(old);xlsModuleSnapshotRelease(current);xlsSyntaxRelease(a);xlsSyntaxRelease(b);xlsSyntaxRelease(other);return 1;
}
typedef struct module_reader {xls_module_store* store;xls_syntax *a,*b;int publish,okay;} module_reader;
static int32 moduleRead(void* user)
{
    module_reader* r=user;r->okay=1;
    uint64_t id=xlsModuleStoreIdentity(r->store,"shared.xl");if(!id){r->okay=0;return 0;}
    for(unsigned i=0;i<128;++i){
        xls_syntax* syntax=i%2 ? r->a : r->b;const xls_text* text=xlsSyntaxText(syntax);
        if(r->publish && !xlsModuleStorePublish(r->store,"shared.xl",syntax)){r->okay=0;break;}
        int exact=0;xls_module_snapshot* view=xlsModuleStoreAcquire(r->store,"shared.xl",xlsTextData(text),xlsTextSize(text),&exact);
        /* Other publishers may evict either source between calls. A retained
         * exact match must match the requested bytes; a miss is an ordinary
         * immutable incremental candidate,never current semantic authority. */
        if(!view || (!r->publish && !exact) || xlsModuleSnapshotIdentity(view)!=id ||
            (exact && strcmp(xlsTextData(xlsSyntaxText(xlsModuleSnapshotSyntax(view))),xlsTextData(text))))r->okay=0;
        XL6_ParseResult copy;xl6ParseResultInit(&copy);if(view && !xlsSyntaxCopyParse(xlsModuleSnapshotSyntax(view),&copy))r->okay=0;xl6ParseResultFree(&copy);xlsModuleSnapshotRelease(view);
        if(!r->okay)break;
    }
    return 0;
}
static int concurrency(void)
{
    xls_module_store* s=xlsModuleStoreCreate(NULL);xls_syntax *a=ordinary("shared.xl",firstSource),*b=ordinary("shared.xl",nextSource);CHECK(s && a && b);
    CHECK(xlsModuleStorePublish(s,"shared.xl",a) && xlsModuleStorePublish(s,"shared.xl",b));
    module_reader readers[4];xthread* threads[4];
    for(unsigned phase=0;phase<2;++phase){
        for(unsigned i=0;i<4;++i){readers[i]=(module_reader){s,a,b,(int)phase,0};threads[i]=xrtThreadCreate(moduleRead,&readers[i],0);CHECK(threads[i]);}
        for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
    }
    xls_module_stats stats;xlsModuleStoreStats(s,&stats);CHECK(stats.modules==1 && stats.versions<=2 && stats.bytes<=stats.limit && stats.hits>=512);
    xlsSyntaxRelease(a);xlsSyntaxRelease(b);xlsModuleStoreRelease(s);return 1;
}
static int sharedWorkers(void)
{
    char folder[160];snprintf(folder,sizeof(folder),"build/module-workers-%llu",(unsigned long long)xlsTestClockMicros());CHECK(xrtDirCreateAll(folder));char* root=xrtPathAbs(folder);char *entry=xrtPathJoin(root,"main.xl"),*dependency=xrtPathJoin(root,"dep.xl"),*project=xrtPathJoin(root,"sample.xlp");CHECK(root && entry && dependency && project);
    const char* main="\xef\xbb\xbfimport dep\r\nfunc main(...) as int\r\n int unique_main=dep.pick_A(7,\"hi\")\r\n return unique_main\r\nend\r\n";
    const char* dep="export func pick_A(int left,string right) as int\r\n int local=left\r\n if local>0 then\r\n return local\r\n end\r\n return 0\r\nend\r\n";
    xls_analysis_document docs[2]={{"main",entry,NULL,1,1},{"dep",dependency,NULL,2,1}};docs[0].text=xlsTextCreate(main,strlen(main),1024*1024,NULL);docs[1].text=xlsTextCreate(dep,strlen(dep),1024*1024,NULL);CHECK(docs[0].text && docs[1].text);
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=entry,.directory=root,.analysisProfile="module-store-fixture",.targetKind="exe"};xls_context* debug=xlsContextCreate(&spec,1024*1024,NULL);spec.configuration="release";xls_context* release=xlsContextCreate(&spec,1024*1024,NULL);CHECK(debug && release);
    xls_module_store* store=xlsModuleStoreCreate(NULL);CHECK(store);xls_worker* workers[3];
    for(unsigned k=0;k<3;++k){workers[k]=xlsWorkerCreateWithModules(NULL,4,NULL,store);CHECK(workers[k]);}
    fixture_result held[3]={0};
    for(unsigned i=0;i<8;++i){
        if(i){xstrbuf changed={0};CHECK(xrtStrBufAppend(&changed,xrtStrView("export func pick_A(int left,string right) as int\r\n int local=left\r\n")));
            for(unsigned j=0;j<i%3;++j)CHECK(xrtStrBufAppendFormat(&changed," int inserted_%u=local+%u\r\n",j,j));
            CHECK(xrtStrBufAppend(&changed,xrtStrView(" if local>0 then\r\n return local\r\n end\r\n return 0\r\nend\r\n")));xlsTextRelease(docs[1].text);docs[1].text=xlsTextCreate(changed.Data,changed.Size,1024*1024,NULL);++docs[1].revision;xrtStrBufFree(&changed);CHECK(docs[1].text);}
        for(unsigned k=0;k<3;++k){fixture_result r=query(workers[k],k,debug,docs,2,"main","main");CHECK(r.state==XLS_QUERY_READY && parity(&r,k,debug,docs,2,"main","main"));if(!k)CHECK(irParity(&r,debug,docs));if(!i)held[k]=r;else releaseResult(&r);}
        xls_module_stats stats;xlsModuleStoreStats(store,&stats);CHECK(stats.modules>=1 && stats.hits>=2*(i+1) && stats.bytes<=stats.limit);
    }
    fixture_result separate=query(workers[0],CHECK_KIND,release,docs,2,"main","main");CHECK(separate.state==XLS_QUERY_READY && parity(&separate,CHECK_KIND,release,docs,2,"main","main") && separate.cache.sessions==2);releaseResult(&separate);
    for(unsigned k=0;k<3;++k){xlsWorkerDestroy(workers[k]);releaseResult(&held[k]);}
    xlsModuleStoreRelease(store);xlsContextRelease(debug);xlsContextRelease(release);xlsTextRelease(docs[0].text);xlsTextRelease(docs[1].text);xrtFree(root);xrtFree(entry);xrtFree(dependency);xrtFree(project);return 1;
}
int main(void)
{checks=0;if(!snapshots() || !pendingAndBudget() || !concurrency() || !sharedWorkers())return 1;printf("analysis_module_store_test passed: %u checks;stable logical IDs,two-version LRU,held snapshots,four readers/publishers,staged cancellation/error/budget fallback,shared native workers/configuration and complete fresh AST/IR parity\n",checks);return 0;}
