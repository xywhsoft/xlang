#include "analysis/frontend_entry_cache.h"
#include "analysis/frontend_internal.h"
#include "analysis/ast_clone.h"
#include "src/sem/xl6_sem.h"
#include "src/stdlib/xl6_stdlib.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static size_t calls,failAt,live;
static int operation(void) {return ++calls!=failAt;}
static void* fixtureCalloc(size_t count,size_t size) {if(!operation()) return NULL;void* p=xrtCalloc(count,size);if(p) ++live;return p;}
static char* fixtureDup(const char* text) {if(!operation()) return NULL;char* p=xrtStrDup(text);if(p) ++live;return p;}
static char* fixtureDupN(const char* text,size_t n) {if(!operation()) return NULL;char* p=xrtStrDupN(text,n);if(p) ++live;return p;}
static char* fixtureAbsolute(const char* path) {if(!operation()) return NULL;char* p=xrtPathAbs(path);if(p) ++live;return p;}
static void fixtureFree(void* p) {if(p) --live;xrtFree(p);}
static XL6_AstNode* fixtureClone(const XL6_AstNode* root,const xls_ast_clone_options* options,size_t* bytes)
{return operation() ? xlsAstClone(root,options,bytes) : NULL;}
#define xrtCalloc fixtureCalloc
#define xrtStrDup fixtureDup
#define xrtStrDupN fixtureDupN
#define xrtPathAbs fixtureAbsolute
#define xrtFree fixtureFree
#define xlsAstClone fixtureClone
#define xlsEntryCacheCreate privateCreate
#define xlsEntryCacheDestroy privateDestroy
#define xlsEntryCacheBytes privateBytes
#define xlsFrontendEntryBegin privateBegin
#define xlsFrontendEntryEnd privateEnd
#define xlsFrontendEntryParse privateParse
#include "analysis/frontend_entry_cache.c"
#undef xrtCalloc
#undef xrtStrDup
#undef xrtStrDupN
#undef xrtPathAbs
#undef xrtFree
#undef xlsAstClone
#undef xlsEntryCacheCreate
#undef xlsEntryCacheDestroy
#undef xlsEntryCacheBytes
#undef xlsFrontendEntryBegin
#undef xlsFrontendEntryEnd
#undef xlsFrontendEntryParse
static unsigned checks;
#define CHECK(e) do {++checks;if(!(e)) {fprintf(stderr,"analysis_entry_cache_test:%d: %s\n",__LINE__,#e);return 0;}}while(0)
static int same(const char* a,const char* b) {return a && b ? !strcmp(a,b) : a==b;}
static int equal(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b) return a==b;
    XL6_AstNode x=*a,y=*b;x.text=y.text=x.doc=y.doc=NULL;x.literalData=y.literalData=NULL;x.children=y.children=NULL;
    x.inferredType=y.inferredType=x.semanticDeclaredType=y.semanticDeclaredType=NULL;x.source=y.source=NULL;x.childCapacity=y.childCapacity=0;
    if(memcmp(&x,&y,sizeof(x)) || !same(a->text,b->text) || !same(a->doc,b->doc) || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)) || !!a->source!=!!b->source ||
       (a->source && (!same(a->source->path,b->source->path) || a->source==b->source || a->source->completionQuery!=b->source->completionQuery ||
                     a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd))) return 0;
    if(!equal(a->inferredType,b->inferredType) || !equal(a->semanticDeclaredType,b->semanticDeclaredType)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!equal(a->children[i],b->children[i])) return 0;
    return 1;
}
static int parsed(xls_entry_cache* cache,const char* path,const char* text,int commit,XL6_ParseResult* result,xls_completion_entry_work* work)
{xls_entry_cache_scope scope;privateBegin(&scope,cache,NULL,path);int okay=privateParse(text,path,result,NULL);*work=privateEnd(&scope,commit);return okay;}
static int primitive(void)
{
    char* path=xrtPathAbs("build/entry-cache-memory.xl");CHECK(path);
    const char* source="#doc\ndesc:\n 文档😀\n#end\nfunc helper<T>(T value) as T\n return value\nend\nclass Box\n string value\nend\nfunc main(...) as int\n string 中文 = \"a\\0😀\"\n auto box = Box()\n int result = helper::<int>(7)\n return result\nend\n";
    xls_entry_cache* cache=privateCreate(1024*1024);CHECK(cache);XL6_ParseResult a,b;xl6ParseResultInit(&a);xl6ParseResultInit(&b);xls_completion_entry_work work;
    CHECK(parsed(cache,path,source,1,&a,&work) && work.parsed==1 && !work.reused && !work.declined && work.bytes>strlen(source));size_t bytes=privateBytes(cache);
    CHECK(xl6StdlibExpandModule(a.root,&a.diags) && xl6SemCheckModuleWithIndex(a.root,&a.diags,"cache",1));xl6ParseResultFree(&a);xl6ParseResultInit(&a);
    CHECK(parsed(cache,path,source,1,&a,&work) && !work.parsed && work.reused==1 && !work.declined);
    CHECK(xl6ParseSource(source,path,&b) && equal(a.root,b.root));xl6ParseResultFree(&a);xl6ParseResultFree(&b);
    for(unsigned i=0;i<2;++i) {
        xls_entry_cache* limited=privateCreate(bytes-i);CHECK(limited);xl6ParseResultInit(&a);
        CHECK(parsed(limited,path,source,1,&a,&work) && work.parsed==1 && !work.reused && work.declined==i && privateBytes(limited)==(i ? 0 : bytes));
        xl6ParseResultFree(&a);privateDestroy(limited);
    }
    xl6ParseResultInit(&a);CHECK(!parsed(cache,path,"func main(\n",1,&a,&work) && work.parsed==1 && privateBytes(cache)==bytes);xl6ParseResultFree(&a);
    xl6ParseResultInit(&a);CHECK(parsed(cache,path,"func main(...) as int\n return 9\nend\n",0,&a,&work) && work.parsed==1 && privateBytes(cache)==bytes);xl6ParseResultFree(&a);
    xl6ParseResultInit(&a);CHECK(parsed(cache,path,source,1,&a,&work) && work.reused==1);xl6ParseResultFree(&a);
    xls_entry_cache_scope outer,inner;privateBegin(&outer,cache,NULL,path);xl6ParseResultInit(&a);CHECK(privateParse(source,path,&a,NULL));
    privateBegin(&inner,cache,NULL,path);xl6ParseResultInit(&b);CHECK(privateParse(source,path,&b,NULL));xls_completion_entry_work nested=privateEnd(&inner,1);
    CHECK(!nested.parsed && !nested.reused && privateEnd(&outer,1).reused==1 && equal(a.root,b.root));xl6ParseResultFree(&a);xl6ParseResultFree(&b);privateDestroy(cache);CHECK(!live);
    /* Every cache allocation/copy seam fails once; the original parser's
       accepted tree survives. A failed hit copy also falls back atomically. */
    calls=0;failAt=0;cache=privateCreate(1024*1024);xl6ParseResultInit(&a);CHECK(parsed(cache,path,source,1,&a,&work));size_t operations=calls;CHECK(operations>=5);xl6ParseResultFree(&a);privateDestroy(cache);CHECK(!live);
    for(size_t failure=1;failure<=operations;++failure) {
        calls=0;failAt=failure;cache=privateCreate(1024*1024);xl6ParseResultInit(&a);xl6ParseResultInit(&b);
        CHECK(parsed(cache,path,source,1,&a,&work) && xl6ParseSource(source,path,&b) && equal(a.root,b.root));xl6ParseResultFree(&a);xl6ParseResultFree(&b);privateDestroy(cache);CHECK(!live);
    }
    failAt=0;cache=privateCreate(1024*1024);xl6ParseResultInit(&a);CHECK(parsed(cache,path,source,1,&a,&work));xl6ParseResultFree(&a);
    calls=0;failAt=2;xl6ParseResultInit(&a);CHECK(parsed(cache,path,source,1,&a,&work) && work.parsed==1 && work.declined==1 && !work.reused && work.bytes==bytes);
    xl6ParseResultFree(&a);privateDestroy(cache);CHECK(!live);failAt=0;xrtFree(path);return 1;
}
static int writeText(const char* path,const char* text) {return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int parity(xls_semantic_completion* result,xls_context* context,const char* entry,const xls_semantic_input* input,size_t at)
{
    xls_semantic_completion* fresh=xlsSemanticCompletionCreateInContext(context,entry,input->path,at,input,1,NULL,NULL,NULL,0);
    char *a=xlsSemanticCompletionJson(result,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);CHECK(result && fresh && a && b && !strcmp(a,b));
    CHECK(!xlsSemanticCompletionEntryWork(fresh).parsed && !xlsSemanticCompletionEntryWork(fresh).reused);
    xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticCompletionDestroy(fresh);return 1;
}
typedef struct cancel_data {unsigned calls,at;} cancel_data;
static int stopAt(void* user) {cancel_data* data=user;return ++data->calls==data->at;}
typedef struct nested_data {xls_context* context;const char* entry;xls_semantic_input input;size_t at;int once,okay;} nested_data;
static int nestedQuery(void* user)
{
    nested_data* data=user;if(data->once++) return 0;
    xls_semantic_check* check=xlsSemanticCheckCreateInContext(data->context,data->entry,&data->input,1,0,NULL,NULL,NULL,0);
    xls_semantic_completion* fresh=xlsSemanticCompletionCreateInContext(data->context,data->entry,data->input.path,data->at,&data->input,1,NULL,NULL,NULL,0);
    data->okay=check && fresh && xlsSemanticCompletionInfo(fresh).available && !xlsSemanticCompletionEntryWork(fresh).parsed && !xlsSemanticCompletionEntryWork(fresh).reused;
    xlsSemanticCheckDestroy(check);xlsSemanticCompletionDestroy(fresh);return 0;
}
static int integrated(void)
{
    char relative[160],error[256];snprintf(relative,sizeof(relative),"build/entry-cache-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
    char *directory=xrtPathAbs(relative),*entry=xrtPathJoin(directory,"main.xl"),*helper=xrtPathJoin(directory,"helper.xl"),*package=xrtPathJoin(directory,"packages/entry_package"),*manifest=xrtPathJoin(package,"xlang.package.json"),*api=xrtPathJoin(package,"api.xl");
    const char* root="\xef\xbb\xbfimport helper\r\nfunc main(...) as int\r\n return helper.answer()\r\nend\r\n";
    CHECK(directory && entry && helper && package && manifest && api && xrtDirCreateAll(package) && writeText(entry,root));
    CHECK(writeText(manifest,"{\"format\":\"xlang-package-1\",\"name\":\"entry_package\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}") && writeText(api,"export func value() as int\n return 7\nend\n"));
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.entry=entry,.directory=directory,.configuration="debug",.analysisProfile="fixture-linked-native",.targetKind="exe",.packagePath="packages"};
    xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_entry_cache* cache=xlsEntryCacheCreate(1024*1024);CHECK(context && cache);
    const char* cursor="import entry_package as ep\nexport func answer() as int\n ep.\n return 0\nend\n";size_t at=(size_t)(strstr(cursor," ep.")-cursor)+4;
    xls_semantic_input input={helper,cursor,strlen(cursor)};
    xls_semantic_completion* initial=xlsSemanticCompletionCreateCachedInContext(context,cache,entry,helper,at,&input,1,NULL,NULL,error,sizeof(error));
    CHECK(parity(initial,context,entry,&input,at) && xlsSemanticCompletionEntryWork(initial).parsed==1 && !xlsSemanticCompletionEntryWork(initial).reused);
    for(unsigned i=0;i<20;++i) {
        char text[256];snprintf(text,sizeof(text),"%s// actual edit %u\n",cursor,i);input=(xls_semantic_input){helper,text,strlen(text)};
        xls_semantic_completion* result=xlsSemanticCompletionCreateCachedInContext(context,cache,entry,helper,at,&input,1,NULL,NULL,error,sizeof(error));
        CHECK(parity(result,context,entry,&input,at) && !xlsSemanticCompletionEntryWork(result).parsed && xlsSemanticCompletionEntryWork(result).reused==1);xlsSemanticCompletionDestroy(result);
    }
    input=(xls_semantic_input){helper,cursor,strlen(cursor)};
    nested_data nested={context,entry,input,at,0,0};
    xls_semantic_completion* result=xlsSemanticCompletionCreateCachedInContext(context,cache,entry,helper,at,&input,1,nestedQuery,&nested,error,sizeof(error));
    CHECK(nested.okay && result && xlsSemanticCompletionEntryWork(result).reused==1 && !xlsSemanticCompletionEntryWork(result).parsed);xlsSemanticCompletionDestroy(result);
    const char* changedRoot="import helper\nfunc main(...) as int\n return helper.answer()+12345\nend\n";
    CHECK(writeText(entry,changedRoot));cancel_data cancel={0,0};
    result=xlsSemanticCompletionCreateInContext(context,entry,helper,at,&input,1,stopAt,&cancel,error,sizeof(error));CHECK(result && cancel.calls>=6);xlsSemanticCompletionDestroy(result);
    unsigned checkpoints=cancel.calls;size_t retained=xlsEntryCacheBytes(cache);
    for(unsigned i=1;i<=checkpoints;++i) {
        CHECK(writeText(entry,changedRoot));cancel=(cancel_data){0,i};
        CHECK(!xlsSemanticCompletionCreateCachedInContext(context,cache,entry,helper,at,&input,1,stopAt,&cancel,error,sizeof(error)) && cancel.calls==i && xlsEntryCacheBytes(cache)==retained);
        CHECK(writeText(entry,root));result=xlsSemanticCompletionCreateCachedInContext(context,cache,entry,helper,at,&input,1,NULL,NULL,error,sizeof(error));
        CHECK(parity(result,context,entry,&input,at) && xlsSemanticCompletionEntryWork(result).reused==1);xlsSemanticCompletionDestroy(result);
    }
    CHECK(writeText(entry,root) && !xlsSemanticCompletionValidate(initial));
    result=xlsSemanticCompletionCreateCachedInContext(context,cache,entry,helper,at,&input,1,NULL,NULL,error,sizeof(error));
    CHECK(parity(result,context,entry,&input,at) && xlsSemanticCompletionEntryWork(result).reused==1);xlsSemanticCompletionDestroy(result);
    CHECK(writeText(entry,"import helper\nfunc main(...) as int\n return helper.answer()+1\nend\n"));
    result=xlsSemanticCompletionCreateCachedInContext(context,cache,entry,helper,at,&input,1,NULL,NULL,error,sizeof(error));CHECK(parity(result,context,entry,&input,at) && xlsSemanticCompletionEntryWork(result).parsed==1);xlsSemanticCompletionDestroy(result);
    CHECK(writeText(api,"export func value() as string\n return \"new\"\nend\n"));
    result=xlsSemanticCompletionCreateCachedInContext(context,cache,entry,helper,at,&input,1,NULL,NULL,error,sizeof(error));xls_completion_item item;
    CHECK(parity(result,context,entry,&input,at) && xlsSemanticCompletionEntryWork(result).reused==1 && xlsSemanticCompletionItem(result,0,&item) && !strcmp(item.detail,"func() as string"));
    xlsContextRelease(context);xlsEntryCacheDestroy(cache);CHECK(xlsSemanticCompletionValidate(result) && xlsSemanticCompletionContext(result));xlsSemanticCompletionDestroy(result);xlsSemanticCompletionDestroy(initial);
    xrtFree(directory);xrtFree(entry);xrtFree(helper);xrtFree(package);xrtFree(manifest);xrtFree(api);return 1;
}
int main(void) {if(!primitive() || !integrated()) return 1;printf("analysis_entry_cache_test passed: %u checks; pre-semantic immutable AST parity, exact budget/fault fallback, nested ownership, real disk/member edits and fresh semantic JSON equivalence\n",checks);return 0;}
