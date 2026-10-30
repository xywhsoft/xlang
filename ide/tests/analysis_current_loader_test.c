#include "analysis/frontend_internal.h"
#include "analysis/frontend_overlay_lookup.h"
#include "analysis/frontend_overlay.h"
#include <stdio.h>
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
#include "analysis/frontend_input_usage.h"
#include "analysis/input_topology.h"
#include "analysis/frontend_raw_body.h"
#include "analysis/frontend_module_graph.h"
#include "analysis_ast_same.h"
#include "src/compile/xl6_imports.h"
#include <wchar.h>
void xlsReferenceSourceReportInit(XL6_SourceReport*);
void xlsReferenceSourceReportFree(XL6_SourceReport*);
int xlsReferenceSourceReportText(XL6_SourceReport*,const char*,const char*,size_t,const char*);
int xlsReferenceSourceReportValidate(XL6_SourceReport*);
int xlsReferenceSourceOverlaySet(XL6_SourceOverlay*,const char*,const char*,size_t);
void xlsReferenceSourceOverlayClear(XL6_SourceOverlay*);
int xlsReferenceSourceOverlayLookup(const XL6_SourceOverlay*,const char*,const XL6_SourceOverlayItem**);
int xlsReferenceSourceOverlayParse(const XL6_SourceOverlay*,const char*,XL6_ParseResult*);
void xlsReferenceImportDepsInit(XL6_ImportDeps*);
void xlsReferenceImportDepsFree(XL6_ImportDeps*);
int xlsReferenceImportsExpandModule(XL6_AstNode*,const char*,XL6_DiagList*,XL6_ImportDeps*);
int xlsReferencePackageResolveImportObserved(const char*,const char*,XL6_PackageResolveResult*,XL6_SourceReport*);
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_current_loader_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static size_t faultSize,faultAt,faultCalls;
void* __real_xrtMalloc(size_t);
void* __wrap_xrtMalloc(size_t size)
{if(faultSize && size==faultSize && ++faultCalls==faultAt)return NULL;return __real_xrtMalloc(size);}
static int sameDiags(const XL6_DiagList* a,const XL6_DiagList* b)
{
    CHECK(a->count==b->count);
    for(size_t i=0;i<a->count;++i)CHECK(a->items[i].line==b->items[i].line && a->items[i].column==b->items[i].column && sameText(a->items[i].message,b->items[i].message) && sameText(a->items[i].file,b->items[i].file));
    return 1;
}
static int sameReport(const XL6_SourceReport* a,const XL6_SourceReport* b)
{
    CHECK(a->complete==b->complete && a->count==b->count && a->probeCount==b->probeCount && a->bytes==b->bytes && a->packagePathObserved==b->packagePathObserved && sameText(a->packagePath,b->packagePath) && sameText(a->errorPath,b->errorPath) && !strcmp(a->error,b->error));
    for(size_t i=0;i<a->count;++i){const XL6_SourceReportItem *x=&a->items[i],*y=&b->items[i];
        CHECK(x->size==y->size && x->rawSize==y->rawSize && x->kind==y->kind && sameText(x->path,y->path) && sameText(x->text,y->text) && sameText(x->origin,y->origin) && !memcmp(x->textSha256,y->textSha256,32) && !memcmp(x->rawSha256,y->rawSha256,32));
        /* Compare the actual original identity proof,not access-time effects
         * from two independent disk reads. */
        CHECK(x->info.Type==y->info.Type && x->info.Available==y->info.Available && x->info.Attributes==y->info.Attributes && x->info.Size==y->info.Size && x->info.Device==y->info.Device && x->info.Identity==y->info.Identity && x->info.Modified==y->info.Modified && x->info.Changed==y->info.Changed);
    }
    for(size_t i=0;i<a->probeCount;++i)CHECK(a->probes[i].exists==b->probes[i].exists && sameText(a->probes[i].path,b->probes[i].path));
    return 1;
}
static int overlayCases(void)
{
    char* path=xrtPathAbs("loader-Ä-中文.xl");char* miss=xrtPathAbs("loader-missing.xl");CHECK(path && miss);
    XL6_SourceReport a,b;xl6SourceReportInit(&a);xlsReferenceSourceReportInit(&b);XL6_SourceOverlay x={.report=&a},y={.report=&b};
    const char* code="func query() as int\n return 7\nend\n";CHECK(xl6SourceOverlaySet(&x,path,code,strlen(code)) && xlsReferenceSourceOverlaySet(&y,path,code,strlen(code)));
    xls_input_usage_scope usage;xlsFrontendInputUsageBegin(&usage);xls_input_topology_scope scope;xlsInputTopologyBegin(&scope,32768);xlsFrontendInputUsageBind(&x,1);
    const XL6_SourceOverlayItem *p=NULL,*q=NULL;CHECK(xlsFrontendOverlayLookup(&x,path,&p)==1 && xlsReferenceSourceOverlayLookup(&y,path,&q)==1 && p && q && p->size==q->size && sameText(p->text,q->text));
    CHECK(xlsFrontendOverlayLookup(&x,miss,&p)==0 && !p && xlsReferenceSourceOverlayLookup(&y,miss,&q)==0 && !q);
    uint64_t used=0;CHECK(xlsFrontendInputUsageEnd(&usage,&used) && used==1);xls_input_topology* topology=xlsInputTopologyEnd(&scope);CHECK(topology && xlsInputTopologyInfo(topology).paths==2);xlsInputTopologyDestroy(topology);
    xlsFrontendInputUsageBegin(&usage);xlsInputTopologyBegin(&scope,32768);xlsFrontendInputUsageBind(&x,1);CHECK(xlsFrontendOverlayLookup(&x,path,&p)==1);
    xl6SourceReportInvalidate(&b,path,"older result changed");CHECK(xlsFrontendInputUsageEnd(&usage,&used) && used==1);topology=xlsInputTopologyEnd(&scope);CHECK(topology && xlsInputTopologyInfo(topology).paths==1);xlsInputTopologyDestroy(topology);
    xlsReferenceSourceReportFree(&b);xlsReferenceSourceReportInit(&b);
    const char bad[]={'C',':','/','\xff',0};
    for(unsigned mode=0;mode<3;++mode){
        if(mode==0){a.complete=b.complete=0;}
        else {xl6SourceReportFree(&a);xlsReferenceSourceReportFree(&b);xl6SourceReportInit(&a);xlsReferenceSourceReportInit(&b);}
        xlsFrontendInputUsageBegin(&usage);xlsInputTopologyBegin(&scope,32768);xlsFrontendInputUsageBind(&x,1);
        const char* input=mode==1 ? bad : path;
        int aa=xlsFrontendOverlayLookup(&x,input,mode==2 ? NULL : &p),bb=xlsReferenceSourceOverlayLookup(&y,input,mode==2 ? NULL : &q);
        CHECK(aa==-1 && aa==bb && sameReport(&a,&b));CHECK(!xlsFrontendInputUsageEnd(&usage,&used) && !used);CHECK(!xlsInputTopologyEnd(&scope));
    }
    XL6_ParseResult left,right;xl6ParseResultInit(&left);xl6ParseResultInit(&right);a.complete=b.complete=0;
    CHECK(!xl6SourceOverlayParse(&x,path,&left) && !xlsReferenceSourceOverlayParse(&y,path,&right) && !left.root && !right.root && sameDiags(&left.diags,&right.diags));
    xl6ParseResultFree(&left);xl6ParseResultFree(&right);xl6SourceOverlayClear(&x);xlsReferenceSourceOverlayClear(&y);xl6SourceReportFree(&a);xlsReferenceSourceReportFree(&b);xrtFree(path);xrtFree(miss);return 1;
}
static int identityFaults(void)
{
#ifdef _WIN32
    char leaf[340];memset(leaf,'a',sizeof(leaf));memcpy(leaf+sizeof(leaf)-5,".xl",4);char* path=xrtPathAbs(leaf);CHECK(path);
    char* alternate=xrtStrDup(path);CHECK(alternate);char* letter=strrchr(alternate,'a');CHECK(letter);*letter='A';
    for(unsigned kind=0;kind<2;++kind)for(unsigned at=1;at<=2;++at){
        XL6_SourceReport a,b;xl6SourceReportInit(&a);xlsReferenceSourceReportInit(&b);XL6_SourceOverlay x={.report=&a},y={.report=&b};
        CHECK(xl6SourceOverlaySet(&x,path,"",0) && xlsReferenceSourceOverlaySet(&y,path,"",0));
        if(kind)CHECK(xl6SourceReportText(&a,path,"",0,"memory") && xlsReferenceSourceReportText(&b,path,"",0,"memory"));
        xls_input_usage_scope usage;xlsFrontendInputUsageBegin(&usage);xlsFrontendInputUsageBind(&x,1);const XL6_SourceOverlayItem *p=NULL,*q=NULL;
        faultSize=(strlen(path)+1)*sizeof(wchar_t);faultAt=at;faultCalls=0;
        int aa=kind ? xl6SourceReportText(&a,alternate,"",0,"memory") : xlsFrontendOverlayLookup(&x,alternate,&p);size_t calls=faultCalls;faultSize=0;
        CHECK(calls==at && aa==(kind ? 0 : -1));uint64_t used=99;CHECK(!xlsFrontendInputUsageEnd(&usage,&used) && !used);
        faultSize=(strlen(path)+1)*sizeof(wchar_t);faultAt=at;faultCalls=0;
        int bb=kind ? xlsReferenceSourceReportText(&b,alternate,"",0,"memory") : xlsReferenceSourceOverlayLookup(&y,alternate,&q);calls=faultCalls;faultSize=0;
        CHECK(calls==at && aa==bb && !a.complete && sameReport(&a,&b));
        xl6SourceOverlayClear(&x);xlsReferenceSourceOverlayClear(&y);xl6SourceReportFree(&a);xlsReferenceSourceReportFree(&b);
    }
    xrtFree(alternate);xrtFree(path);
#endif
    return 1;
}
static int borrowedFailure(void)
{
#ifdef _WIN32
    char leaf[340];memset(leaf,'c',sizeof(leaf));memcpy(leaf+sizeof(leaf)-5,".xl",4);char* first=xrtPathAbs(leaf);CHECK(first);char* second=xrtStrDup(first);CHECK(second);char* last=strrchr(second,'c');CHECK(last);*last='d';
    xls_text *a=xlsTextCreate("// first\n",9,1024,NULL),*b=xlsTextCreate("// second\n",10,1024,NULL);CHECK(a && b);
    xls_analysis_document docs[]={{"first",first,a,1,1},{"second",second,b,1,1}};xls_input_batch* batch=xlsInputBatchCreate(NULL,NULL,docs,2,"first",NULL,NULL,NULL,0);CHECK(batch);
    XL6_SourceOverlay overlay={0};faultSize=(strlen(first)+1)*sizeof(wchar_t);faultAt=1;faultCalls=0;int okay=xlsFrontendOverlayBorrow(&overlay,batch,NULL,NULL);size_t calls=faultCalls;faultSize=0;
    CHECK(okay==0 && calls==1 && overlay.count==1);xlsFrontendOverlayClear(&overlay,batch);CHECK(!strcmp(xlsTextData(a),"// first\n") && !strcmp(xlsTextData(b),"// second\n"));
    xlsInputBatchDestroy(batch);xlsTextRelease(a);xlsTextRelease(b);xrtFree(first);xrtFree(second);
#endif
    return 1;
}
static int sameList(const XL6_PackageStringList* a,const XL6_PackageStringList* b)
{CHECK(a->count==b->count);for(size_t i=0;i<a->count;++i)CHECK(sameText(a->items[i],b->items[i]));return 1;}
static int packageCase(void)
{
    char* entry=xrtPathJoin(XLS_CURRENT_LOADER_DIRECTORY,"main.xl");char* folder=xrtPathJoin(XLS_CURRENT_LOADER_DIRECTORY,"pkg");char* manifest=folder ? xrtPathJoin(folder,"xlang.package.json") : NULL;char* api=folder ? xrtPathJoin(folder,"api.xl") : NULL;CHECK(entry && folder && manifest && api && xrtDirCreateAll(folder));
    const char* main="import pkg\nfunc main() as int\n return pkg.value()\nend\n";const char* body="export func value() as int\n return 7\nend\n";
    const char* json="{\"format\":\"xlang-package-1\",\"name\":\"pkg\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"],\"runtimeSymbols\":[\"xl6_fixture_symbol\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}";
    CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)main,strlen(main)}) && xrtFileWriteAtomic(api,(xbytesview){(const unsigned char*)body,strlen(body)}) && xrtFileWriteAtomic(manifest,(xbytesview){(const unsigned char*)json,strlen(json)}));
    XL6_ParseResult a,b;xl6ParseResultInit(&a);xl6ParseResultInit(&b);XL6_ImportDeps x,y;xl6ImportDepsInit(&x);xlsReferenceImportDepsInit(&y);XL6_SourceReport p,q;xl6SourceReportInit(&p);xlsReferenceSourceReportInit(&q);XL6_SourceOverlay left={.report=&p},right={.report=&q};x.sourceOverlay=&left;y.sourceOverlay=&right;
    CHECK(xl6SourceOverlayParse(&left,entry,&a) && xlsReferenceSourceOverlayParse(&right,entry,&b));
    int aa=xl6ImportsExpandModule(a.root,entry,&a.diags,&x),bb=xlsReferenceImportsExpandModule(b.root,entry,&b.diags,&y);
    if(!aa || !bb){fprintf(stderr,"loader package acceptance %d/%d\n",aa,bb);for(size_t i=0;i<a.diags.count;++i)fprintf(stderr,"loader: %s\n",a.diags.items[i].message);}
    CHECK(aa && aa==bb && sameAst(a.root,b.root) && sameDiags(&a.diags,&b.diags) && sameReport(&p,&q));
    CHECK(x.packageInfoCount==1 && y.packageInfoCount==1 && x.runtimeSymbols.count==1 && sameText(x.runtimeSymbols.items[0],"xl6_fixture_symbol") && sameList(&x.runtimeSymbols,&y.runtimeSymbols));
    CHECK(sameText(x.packageInfos[0].name,y.packageInfos[0].name) && sameText(x.packageInfos[0].version,y.packageInfos[0].version) && sameText(x.packageInfos[0].abiVersion,y.packageInfos[0].abiVersion) && sameText(x.packageInfos[0].dir,y.packageInfos[0].dir) && x.packageInfos[0].compiledRoot==y.packageInfos[0].compiledRoot && sameList(&x.packageInfos[0].dependencies,&y.packageInfos[0].dependencies));
    xerror* sentinel=xrtErrorBuild(&(xerrordesc){.Kind=XERR_ARGUMENT,.Domain="loader.fixture",.Code=17,.Message="preserved probe owner"});CHECK(sentinel);xrtSetErrorTake(sentinel);
    XL6_PackageResolveResult first,second;xl6PackageResolveResultInit(&first);xl6PackageResolveResultInit(&second);
    int resolved=xl6PackageResolveImportObserved(entry,"main.xl",&first,NULL);CHECK(xrtGetError()==sentinel);
    int original=xlsReferencePackageResolveImportObserved(entry,"main.xl",&second,NULL);CHECK(resolved==original && xrtGetError()==sentinel && first.resolved==second.resolved && first.kind==second.kind);
    xl6PackageResolveResultFree(&first);xl6PackageResolveResultFree(&second);xrtClearError();
    CHECK(xrtFileWriteAtomic(api,(xbytesview){(const unsigned char*)"// changed",10}));CHECK(!xl6SourceReportValidate(&p) && !xlsReferenceSourceReportValidate(&q));
    xl6ImportDepsFree(&x);xlsReferenceImportDepsFree(&y);xl6SourceReportFree(&p);xlsReferenceSourceReportFree(&q);xl6ParseResultFree(&a);xl6ParseResultFree(&b);xrtFree(entry);xrtFree(folder);xrtFree(manifest);xrtFree(api);return 1;
}
static int compiledCase(void)
{
    unsigned char wire[40]={0};memcpy(wire,"XLP1",4);wire[4]=(unsigned char)XL6_AST_MODULE;
    XL_CompiledPackage package={.name="compiled_fixture",.interfaceData=wire,.interfaceSize=sizeof(wire)};XL6_CompiledPackageSet set={.items=&package,.count=1};
    XL6_ParseResult a,b;xl6ParseResultInit(&a);xl6ParseResultInit(&b);char* path=xrtPathAbs("compiled-loader.xl");CHECK(path && xl6ParseSource("import compiled_fixture\n",path,&a) && xl6ParseSource("import compiled_fixture\n",path,&b));
    XL6_ImportDeps x,y;xl6ImportDepsInit(&x);xlsReferenceImportDepsInit(&y);x.compiledPackages=y.compiledPackages=&set;
    xls_frontend_context_scope call;xlsFrontendContextBegin(&call,NULL);xls_raw_scope raw;xlsRawBodyScopeBegin(&raw,NULL,1024*1024);CHECK(raw.ready && raw.environmentReady);
    xls_module_store* store=xlsModuleStoreCreate(&(xls_module_limits){16,4,1024*1024});CHECK(store);xls_module_graph_scope graph;xlsModuleGraphBegin(&graph,store,NULL,1024*1024);
    XL6_SourceOverlay overlay={0};xls_input_usage_scope usage;xlsFrontendInputUsageBegin(&usage);xlsFrontendInputUsageBind(&overlay,0);
    int aa=xl6ImportsExpandModule(a.root,path,&a.diags,&x);uint64_t used=99;CHECK(aa && !raw.environmentReady && !xlsFrontendInputUsageEnd(&usage,&used) && !used);XL6_SourceReport report;xl6SourceReportInit(&report);
    const char* entry="import compiled_fixture\n";CHECK(xl6SourceReportText(&report,path,entry,strlen(entry),"memory"));
    CHECK(!xlsModuleGraphEnd(&graph,&report));xl6SourceReportFree(&report);
    xlsRawBodyScopeEnd(&raw);CHECK(xlsFrontendContextEnd(&call));xlsModuleStoreRelease(store);
    int bb=xlsReferenceImportsExpandModule(b.root,path,&b.diags,&y);CHECK(aa==bb && sameAst(a.root,b.root) && sameDiags(&a.diags,&b.diags) && x.features==y.features && (x.features&XL6_IMPORT_FEATURE_COMPILED_PACKAGE_CONSUMER));
    xl6ImportDepsFree(&x);xlsReferenceImportDepsFree(&y);xl6ParseResultFree(&a);xl6ParseResultFree(&b);xrtFree(path);return 1;
}
int main(void)
{
    if(!overlayCases() || !identityFaults() || !borrowedFailure() || !packageCase() || !compiledCase())return 1;
    printf("analysis_current_loader_test passed: %u checks; original tri-state/report/AST contracts,real identity allocation failures,package metadata and unproven virtual dependency refusal\n",checks);return 0;
}
#else
int main(void){puts("analysis_current_loader_test passed: legacy cohort has no current tri-state API");return 0;}
#endif
