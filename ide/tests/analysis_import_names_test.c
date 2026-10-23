#include "analysis/check.h"
#include "analysis/frontend_import_names.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static size_t calls,failAt;
static int operation(void) {return ++calls!=failAt;}
static void* fixtureCalloc(size_t count,size_t size) {return operation() ? xrtCalloc(count,size) : NULL;}
static bool fixtureMapInit(xmap* map,size_t bytes) {return operation() && xrtMapInit(map,bytes);}
static bool fixtureMapSet(xmap* map,xbytesview key,const void* value) {return operation() && xrtMapSet(map,key,value);}
#define xrtCalloc fixtureCalloc
#define xrtMapInit fixtureMapInit
#define xrtMapSet fixtureMapSet
#define xl6ImportDepsInit privateDepsInit
#define xl6ImportDepsFree privateDepsFree
#define xl6ImportsExpandModule privateExpand
#define xlsFrontendImportNamesBegin privateBegin
#define xlsFrontendImportNamesEnd privateEnd
/* The included upstream TU contains its existing unused helpers/local. Keep
 * the fixture itself strict; use the shared source's established allowances. */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include "xl6_imports.c"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#undef xrtCalloc
#undef xrtMapInit
#undef xrtMapSet
#undef xl6ImportDepsInit
#undef xl6ImportDepsFree
#undef xl6ImportsExpandModule
#undef xlsFrontendImportNamesBegin
#undef xlsFrontendImportNamesEnd
static unsigned checks;
#define CHECK(value) do {++checks;if(!(value)) {fprintf(stderr,"analysis_import_names_test:%d: %s\n",__LINE__,#value);return 0;}}while(0)
static int same(const char* a,const char* b) {return a && b ? !strcmp(a,b) : a==b;}
static int lookup(XL6_ImportNameMap* map,const char* name)
{
    CHECK(same(xl6ImportsNameMapFind(map,name),xlsImportsNameMapFindOriginal(map,name)));
    char *a=xl6ImportsNameMapRewriteDotted(map,name),*b=xlsImportsNameMapRewriteDottedOriginal(map,name);
    int equal=same(a,b);xrtFree(a);xrtFree(b);CHECK(equal);return 1;
}
static int fill(XL6_ImportNameMap* map,size_t count)
{
    xl6ImportsNameMapInit(map);
    for(size_t i=0;i<count;++i) {char name[64],value[64];snprintf(name,sizeof(name),"name%zu",i);snprintf(value,sizeof(value),"private%zu",i);if(!xl6ImportsNameMapAdd(map,name,value)) return 0;}
    return 1;
}
static int primitive(void)
{
    xls_import_names_scope scope;privateBegin(&scope,8u*1024u*1024u);XL6_ImportNameMap map;CHECK(fill(&map,128));
    for(unsigned i=0;i<128;++i) {char name[64];snprintf(name,sizeof(name),"name%u",i);CHECK(lookup(&map,name));snprintf(name,sizeof(name),"name%u.member",i);CHECK(lookup(&map,name));}
    CHECK(lookup(&map,"missing") && lookup(&map,"missing.a.b") && lookup(&map,"name12x.member") && lookup(&map,"NAME12.member") && lookup(&map,NULL));
    CHECK(xl6ImportsNameMapAdd(&map,"name12","ignored") && map.count==128 && !strcmp(xl6ImportsNameMapFind(&map,"name12"),"private12"));
    size_t exact=map.xlsIndex->bytes;CHECK(map.xlsIndex->enabled && map.xlsIndex->indexed==map.count);
    xls_import_names_scope nested;privateBegin(&nested,0);CHECK(lookup(&map,"name12.member"));xls_import_names_work work;privateEnd(&nested,&work);CHECK(!work.lookups && work.fallbacks==2);
    CHECK(lookup(&map,"name127.member"));xl6ImportsNameMapFree(&map);privateEnd(&scope,&work);CHECK(work.indexedRows==128 && work.lookups>256 && work.noDotSkips>=128 && work.maxBytes==exact);
    for(unsigned shortfall=0;shortfall<2;++shortfall) {
        privateBegin(&scope,exact-shortfall);CHECK(fill(&map,128) && lookup(&map,"name127"));
        CHECK(map.xlsIndex && map.xlsIndex->enabled==!shortfall && (!shortfall ? map.xlsIndex->bytes==exact : !map.xlsIndex->rows.Count));
        xl6ImportsNameMapFree(&map);privateEnd(&scope,&work);CHECK(work.maxBytes<=exact-shortfall);
    }
    /* Both insertion orders matter: the original selects first matching
       prefix, rather than longest prefix. Unicode remains byte-exact. */
    for(unsigned reverse=0;reverse<2;++reverse) {
        CHECK(fill(&map,20));CHECK(xl6ImportsNameMapAdd(&map,reverse ? "a.b" : "a","first") && xl6ImportsNameMapAdd(&map,reverse ? "a" : "a.b","second"));
        CHECK(lookup(&map,"a.b.c") && lookup(&map,"a.b") && lookup(&map,"a.bc") && lookup(&map,"a"));
        CHECK(!strcmp(xl6ImportsNameMapFind(&map,"a.b"),reverse ? "first" : "second"));
        char* value=xl6ImportsNameMapRewriteDotted(&map,"a.b.c");CHECK(value && !strcmp(value,reverse ? "first.c" : "first.b.c"));xrtFree(value);
        CHECK(xl6ImportsNameMapAdd(&map,"中文😀","unicode") && lookup(&map,"中文😀.类型.成员") && lookup(&map,"中文.类型"));xl6ImportsNameMapFree(&map);
    }
    /* Fail every index allocation/container operation. Original map writes
       and all lookup/rewrite answers survive through the untouched scans. */
    calls=0;failAt=0;CHECK(fill(&map,32) && lookup(&map,"name31.member"));size_t operations=calls;xl6ImportsNameMapFree(&map);CHECK(operations>=34);
    for(size_t failure=1;failure<=operations;++failure) {
        calls=0;failAt=failure;CHECK(fill(&map,32) && lookup(&map,"name31.member") && lookup(&map,"missing") && lookup(&map,"name17"));
        xl6ImportsNameMapFree(&map);CHECK(!map.items && !map.count && !map.xlsIndex);
    }
    failAt=0;xl6ImportsNameMapInit(&map);map.count=65537;map.items=xrtCalloc(map.count,sizeof(*map.items));CHECK(map.items);
    map.items[0]=(XL6_ImportNameMapItem){xrtStrDup("first"),xrtStrDup("one")};map.items[65536]=(XL6_ImportNameMapItem){xrtStrDup("last"),xrtStrDup("two")};
    CHECK(!strcmp(xl6ImportsNameMapFind(&map,"last"),"two") && !map.xlsIndex->enabled);xl6ImportsNameMapFree(&map);return 1;
}
static int writeText(const char* path,const char* body) {return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)});}
static int pair(xls_context* context,const char* entry,const xls_semantic_input* input,int accepted)
{
    char error[256];xls_import_names_scope scope;xls_import_names_work fastWork,slowWork;
    xlsFrontendImportNamesBegin(&scope,8u*1024u*1024u);xls_semantic_check* fast=xlsSemanticCheckCreateInContext(context,entry,input,1,1,NULL,NULL,error,sizeof(error));xlsFrontendImportNamesEnd(&scope,&fastWork);
    xlsFrontendImportNamesBegin(&scope,0);xls_semantic_check* slow=xlsSemanticCheckCreateInContext(context,entry,input,1,1,NULL,NULL,error,sizeof(error));xlsFrontendImportNamesEnd(&scope,&slowWork);
    char *a=xlsSemanticCheckJson(fast,NULL),*b=xlsSemanticCheckJson(slow,NULL);CHECK(fast && slow && a && b && !strcmp(a,b));
    CHECK(xlsSemanticCheckInfo(fast).accepted==accepted && xlsSemanticCheckValidate(fast) && xlsSemanticCheckValidate(slow));
    CHECK(fastWork.indexedRows>=128 && fastWork.lookups>0 && fastWork.noDotSkips>0 && !slowWork.indexedRows && !slowWork.lookups && slowWork.fallbacks>0);
    xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticCheckDestroy(fast);xlsSemanticCheckDestroy(slow);return 1;
}
typedef struct thread_input {xls_context* context;const char* entry;xls_semantic_input input;size_t budget;xatomic32* arrived;int okay;} thread_input;
static int32 threaded(void* user)
{
    thread_input* data=user;xls_import_names_scope scope;xls_import_names_work work;xlsFrontendImportNamesBegin(&scope,data->budget);
    xrtAtomic32FetchAdd(data->arrived,1,XMEMORY_ACQ_REL);while(xrtAtomic32Load(data->arrived,XMEMORY_ACQUIRE)<2) xrtThreadYield();
    xls_semantic_check* result=xlsSemanticCheckCreateInContext(data->context,data->entry,&data->input,1,1,NULL,NULL,NULL,0);
    xlsFrontendImportNamesEnd(&scope,&work);data->okay=result && xlsSemanticCheckInfo(result).accepted && (data->budget ? work.indexedRows>=128 && work.lookups : !work.indexedRows && !work.lookups && work.fallbacks);
    xlsSemanticCheckDestroy(result);return 0;
}
static int integrated(void)
{
    char relative[160];snprintf(relative,sizeof(relative),"build/import-names-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
    char *directory=xrtPathAbs(relative),*entry=xrtPathJoin(directory,"main.xl"),*dep=xrtPathJoin(directory,"names_dep.xl");CHECK(directory && entry && dep && xrtDirCreateAll(directory));
    xstrbuf source;xrtStrBufInit(&source);int okay=1;
    for(unsigned i=0;i<128;++i) {char text[128];snprintf(text,sizeof(text),"func local%u() as int\n return %u\nend\n",i,i);if(!xrtStrBufAppend(&source,xrtStrView(text))) okay=0;}
    CHECK(okay && xrtStrBufAppend(&source,xrtStrView("func compute(int value) as int\n return value+1\nend\nexport func value() as int\n return compute(7)\nend\n")) && writeText(dep,source.Data));
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.entry=entry,.directory=directory,.configuration="debug",.analysisProfile="fixture-linked-native",.targetKind="exe"};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);CHECK(context);
    const char* bodies[]={"import names_dep as nd\nfunc main(...) as int\n return nd.value()\nend\n",
        "import names_dep as nd\nfunc local(int nd) as int\n return nd+1\nend\nfunc main(...) as int\n return nd.value()+local(1)\nend\n",
        "import names_dep as nd\nfunc main(...) as int\n return nd.missing()\nend\n"};
    for(unsigned i=0;i<3;++i) {xls_semantic_input input={entry,bodies[i],strlen(bodies[i])};CHECK(pair(context,entry,&input,i!=2));}
    xatomic32 arrived;xrtAtomic32Init(&arrived,0);thread_input readers[2];xthread* threads[2];
    for(unsigned i=0;i<2;++i) {readers[i]=(thread_input){context,entry,{entry,bodies[0],strlen(bodies[0])},i ? 0 : 8u*1024u*1024u,&arrived,0};threads[i]=xrtThreadCreate(threaded,&readers[i],0);CHECK(threads[i]);}
    for(unsigned i=0;i<2;++i) {CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
    xlsContextRelease(context);xrtStrBufFree(&source);xrtFree(directory);xrtFree(entry);xrtFree(dep);return 1;
}
int main(void) {if(!primitive() || !integrated()) return 1;printf("analysis_import_names_test passed: %u checks; original exact/dotted first-match parity,Unicode,budget/fault/append/lifetime,full semantic JSON/index/IR and concurrent TLS reference isolation\n",checks);return 0;}
