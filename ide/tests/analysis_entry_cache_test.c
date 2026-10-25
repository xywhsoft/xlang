#include "analysis/frontend_entry_cache.h"
#include "analysis/frontend_internal.h"
#include "analysis/check_session.h"
#include "analysis/ast_clone.h"
#include "src/sem/xl6_sem.h"
#include "src/sem/xl6_completion.h"
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
static XL6_AstNode* fixtureMaterialize(const XL6_AstNode* root,const xls_ast_clone_options* options,size_t* bytes)
{return operation() ? xlsAstMaterialize(root,options,bytes) : NULL;}
static bool fixtureMapInit(xmap* map,size_t bytes) {return operation() && xrtMapInit(map,bytes);}
static bool fixtureMapDrop(xmap* map,xmapdrop drop,void* user) {return operation() && xrtMapSetDrop(map,drop,user);}
static bool fixtureMapSet(xmap* map,xbytesview key,const void* value) {return operation() && xrtMapSet(map,key,value);}
static void* fixtureMapAdd(xmap* map,xbytesview key,bool* added) {return operation() ? xrtMapGetOrAdd(map,key,added) : NULL;}
static xls_text* fixtureText(const char* text,size_t length,size_t limit,enum xls_text_error* error)
{return operation() ? xlsTextCreate(text,length,limit,error) : NULL;}
static xls_syntax* fixtureAdopt(const char* path,xls_text* text,XL6_AstNode** root)
{return operation() ? xlsSyntaxAdoptOrdinary(path,text,root) : NULL;}
static xls_syntax* fixtureFunction(const char* path,xls_text* text,const xls_syntax* old,xls_parse_work* work)
{return operation() ? xlsSyntaxTryFunction(path,text,old,work) : NULL;}
static int fixtureCopy(const xls_syntax* syntax,XL6_ParseResult* result)
{return operation() && xlsSyntaxCopyParse(syntax,result);}
static int fixtureCursorCopy(const xls_syntax* syntax,XL6_CompletionQuery* query,XL6_ParseResult* result)
{return operation() && xlsSyntaxCopyCursorParse(syntax,query,result);}
static int fixtureStorage(const xls_syntax* syntax,size_t limit,size_t* bytes)
{return operation() && xlsSyntaxCacheBytes(syntax,limit,bytes);}
#define xlsTextCreate fixtureText
#define xlsSyntaxAdoptOrdinary fixtureAdopt
#define xlsSyntaxTryFunction fixtureFunction
#define xlsSyntaxCopyParse fixtureCopy
#define xlsSyntaxCopyCursorParse fixtureCursorCopy
#define xlsSyntaxCacheBytes fixtureStorage
#define xrtMapInit fixtureMapInit
#define xrtMapSetDrop fixtureMapDrop
#define xrtMapSet fixtureMapSet
#define xrtMapGetOrAdd fixtureMapAdd
#define xrtCalloc fixtureCalloc
#define xrtStrDup fixtureDup
#define xrtStrDupN fixtureDupN
#define xrtPathAbs fixtureAbsolute
#define xrtFree fixtureFree
#define xlsAstClone fixtureClone
#define xlsAstMaterialize fixtureMaterialize
#define xlsEntryCacheCreate privateCreate
#define xlsEntryCacheDestroy privateDestroy
#define xlsEntryCacheBytes privateBytes
#define xlsModuleCacheBytes privateModuleBytes
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
#undef xlsAstMaterialize
#undef xlsTextCreate
#undef xlsSyntaxAdoptOrdinary
#undef xlsSyntaxTryFunction
#undef xlsSyntaxCopyParse
#undef xlsSyntaxCopyCursorParse
#undef xlsSyntaxCacheBytes
#undef xrtMapInit
#undef xrtMapSetDrop
#undef xrtMapSet
#undef xrtMapGetOrAdd
#undef xlsEntryCacheCreate
#undef xlsEntryCacheDestroy
#undef xlsEntryCacheBytes
#undef xlsModuleCacheBytes
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
    /* Allocation ownership differs between pooled cached checks and fresh
     * heap trees. All language/source fields below still compare exactly. */
    x.xlsStorage=y.xlsStorage=NULL;
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
    CHECK(!xlsSemanticCompletionModuleWork(fresh).parsed && !xlsSemanticCompletionModuleWork(fresh).reused);
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
        CHECK(parity(result,context,entry,&input,at) && !xlsSemanticCompletionEntryWork(result).parsed && xlsSemanticCompletionEntryWork(result).reused==1);
        CHECK(!xlsSemanticCompletionModuleWork(result).parsed && xlsSemanticCompletionModuleWork(result).reused==1 && xlsSemanticCompletionModuleWork(result).bytes>0);xlsSemanticCompletionDestroy(result);
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
static int checkParity(xls_semantic_check* result,xls_context* context,const char* entry,const xls_semantic_input* input,size_t count)
{
    xls_semantic_check* fresh=xlsSemanticCheckCreateInContext(context,entry,input,count,1,NULL,NULL,NULL,0);
    char *a=xlsSemanticCheckJson(result,NULL),*b=xlsSemanticCheckJson(fresh,NULL);
    CHECK(result && fresh && a && b && !strcmp(a,b) && xlsSemanticCheckValidate(result));
    CHECK(!xlsSemanticCheckEntryWork(fresh).parsed && !xlsSemanticCheckEntryWork(fresh).reused);
    CHECK(!xlsSemanticCheckModuleWork(fresh).parsed && !xlsSemanticCheckModuleWork(fresh).reused);
    if(xlsSemanticCheckInfo(result).accepted) {
        CHECK(equal(xlsSemanticCheckAst(result),xlsSemanticCheckAst(fresh)));
        XL6_Output left,right;CHECK(xl6OutputOpenMemory(&left) && xl6OutputOpenMemory(&right));
        xl6IrDumpModuleJson(&left,xlsSemanticCheckIr(result));xl6IrDumpModuleJson(&right,xlsSemanticCheckIr(fresh));
        char *x=xl6OutputHasFailed(&left) ? NULL : xl6OutputTakeMemoryText(&left,NULL);
        char *y=xl6OutputHasFailed(&right) ? NULL : xl6OutputTakeMemoryText(&right,NULL);
        CHECK(x && y && !strcmp(x,y));xrtFree(x);xrtFree(y);xl6OutputDiscard(&left);xl6OutputDiscard(&right);
    }
    xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticCheckDestroy(fresh);return 1;
}
static int checkIntegrated(void)
{
    char relative[160],error[256];snprintf(relative,sizeof(relative),"build/check-entry-cache-%llu-%llu",(unsigned long long)xrtThreadCurrentId(),(unsigned long long)xrtClock());
    char *directory=xrtPathAbs(relative),*entry=xrtPathJoin(directory,"main.xl"),*helper=xrtPathJoin(directory,"helper.xl"),*package=xrtPathJoin(directory,"packages/check_package"),*manifest=xrtPathJoin(package,"xlang.package.json"),*api=xrtPathJoin(package,"api.xl");
    const char* root="\xef\xbb\xbfimport helper\r\nfunc main(...) as int\r\n return helper.answer()\r\nend\r\n";
    const char* body="import check_package as cp\nexport func answer() as int\n return cp.value()\nend\n";
    const char* goodApi="export func value() as int\n return 7\nend\n";
    CHECK(directory && entry && helper && package && manifest && api && xrtDirCreateAll(package) && writeText(entry,root));
    CHECK(writeText(manifest,"{\"format\":\"xlang-package-1\",\"name\":\"check_package\",\"version\":\"1.0.0\",\"abiVersion\":\"1\",\"kind\":\"source\",\"artifact\":{\"kind\":\"source\",\"xlSources\":[\"api.xl\"]},\"dependencies\":[],\"links\":[],\"loadlibs\":[]}") && writeText(api,goodApi));
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.entry=entry,.directory=directory,.configuration="debug",.analysisProfile="fixture-linked-native",.targetKind="exe",.packagePath="packages"};
    xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(context && session);
    xls_text* text=xlsTextCreate(body,strlen(body),8u*1024u*1024u,NULL);CHECK(text);
    xls_analysis_document document={.key="member",.path=helper,.text=text,.identity=1,.revision=1};xls_semantic_input input={helper,body,strlen(body)};
    xls_semantic_check* initial=xlsCheckSessionCheckInContext(session,context,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
    CHECK(checkParity(initial,context,entry,&input,1) && xlsSemanticCheckInfo(initial).accepted && xlsSemanticCheckEntryWork(initial).parsed==1 && !xlsSemanticCheckEntryWork(initial).reused);
    xlsSemanticCheckDestroy(initial);initial=NULL;xlsTextRelease(text);text=NULL;
    for(unsigned i=0;i<20;++i) {
        char edited[256];snprintf(edited,sizeof(edited),"%s// member edit %u 中文😀\n",body,i);text=xlsTextCreate(edited,strlen(edited),8u*1024u*1024u,NULL);CHECK(text);
        document.text=text;document.revision=i+2;input=(xls_semantic_input){helper,edited,strlen(edited)};
        xls_semantic_check* result=xlsCheckSessionCheckInContext(session,context,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
        CHECK(checkParity(result,context,entry,&input,1) && xlsSemanticCheckInfo(result).accepted && !xlsSemanticCheckEntryWork(result).parsed && xlsSemanticCheckEntryWork(result).reused==1);
        CHECK(xlsSemanticCheckModuleWork(result).parsed==1 && xlsSemanticCheckModuleWork(result).reused==1 && xlsSemanticCheckModuleWork(result).bytes>0);
        xls_check_stats stats;xlsCheckSessionStats(session,&stats);CHECK(stats.builds==i+2 && !stats.hits && !stats.entryFullParses && !stats.entrySyntaxHits);
        xlsSemanticCheckDestroy(result);xlsTextRelease(text);
    }
    text=xlsTextCreate(body,strlen(body),8u*1024u*1024u,NULL);CHECK(text);document.text=text;document.revision=22;input=(xls_semantic_input){helper,body,strlen(body)};
    initial=xlsCheckSessionCheckInContext(session,context,&document,1,NULL,1,NULL,NULL,error,sizeof(error));CHECK(checkParity(initial,context,entry,&input,1));
    xls_semantic_check* result=xlsCheckSessionCheckInContext(session,context,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
    CHECK(result==initial && xlsSemanticCheckEntryWork(result).reused==1);xlsSemanticCheckDestroy(result);
    CHECK(writeText(entry,root) && !xlsSemanticCheckValidate(initial));
    result=xlsCheckSessionCheckInContext(session,context,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
    CHECK(checkParity(result,context,entry,&input,1) && xlsSemanticCheckEntryWork(result).reused==1);xlsSemanticCheckDestroy(result);
    CHECK(writeText(api,"export func value() as string\n return \"changed\"\nend\n"));
    result=xlsCheckSessionCheckInContext(session,context,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
    CHECK(checkParity(result,context,entry,&input,1) && !xlsSemanticCheckInfo(result).accepted && xlsSemanticCheckInfo(result).diagnostics && xlsSemanticCheckEntryWork(result).reused==1);xlsSemanticCheckDestroy(result);
    CHECK(writeText(api,goodApi) && writeText(entry,"import helper\nfunc main(\n"));
    result=xlsCheckSessionCheckInContext(session,context,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
    CHECK(checkParity(result,context,entry,&input,1) && !xlsSemanticCheckInfo(result).accepted && xlsSemanticCheckEntryWork(result).parsed==1);xlsSemanticCheckDestroy(result);
    CHECK(writeText(entry,root));result=xlsCheckSessionCheckInContext(session,context,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
    CHECK(checkParity(result,context,entry,&input,1) && xlsSemanticCheckInfo(result).accepted && xlsSemanticCheckEntryWork(result).reused==1);xlsSemanticCheckDestroy(result);
    spec.configuration="release";xls_context* changed=xlsContextCreate(&spec,1024*1024,NULL);CHECK(changed);
    result=xlsCheckSessionCheckInContext(session,changed,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
    CHECK(checkParity(result,changed,entry,&input,1) && xlsSemanticCheckEntryWork(result).parsed==1 && !xlsSemanticCheckEntryWork(result).reused);xlsSemanticCheckDestroy(result);
    xls_text* opened=xlsTextCreate(root,strlen(root),8u*1024u*1024u,NULL);CHECK(opened);xls_analysis_document documents[]={document,{.key="entry",.path=entry,.text=opened,.identity=2,.revision=1}};
    result=xlsCheckSessionCheckInContext(session,changed,documents,2,"entry",1,NULL,NULL,error,sizeof(error));
    xls_semantic_input both[]={input,{entry,root,strlen(root)}};
    CHECK(checkParity(result,changed,entry,both,2) && xlsSemanticCheckInfo(result).accepted && !xlsSemanticCheckEntryWork(result).parsed && !xlsSemanticCheckEntryWork(result).reused);xlsSemanticCheckDestroy(result);
    result=xlsCheckSessionCheckInContext(session,changed,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
    CHECK(checkParity(result,changed,entry,&input,1) && xlsSemanticCheckEntryWork(result).reused==1);xlsSemanticCheckDestroy(result);xlsTextRelease(opened);
    /* Budget fallback still runs the full fresh compiler, with a valid report. */
    xls_entry_cache* limited=xlsEntryCacheCreate(1);CHECK(limited);
    result=xlsSemanticCheckCreateCachedInContext(context,limited,entry,&input,1,1,NULL,NULL,NULL,NULL,error,sizeof(error));
    CHECK(checkParity(result,context,entry,&input,1) && xlsSemanticCheckInfo(result).accepted && xlsSemanticCheckEntryWork(result).parsed==1 && xlsSemanticCheckEntryWork(result).declined==1 && !xlsSemanticCheckEntryWork(result).bytes);
    xlsSemanticCheckDestroy(result);xlsEntryCacheDestroy(limited);
    /* Every actual cancellation point drops pending new-entry captures. */
    xls_entry_cache* cache=xlsEntryCacheCreate(1024*1024);CHECK(cache);
    result=xlsSemanticCheckCreateCachedInContext(context,cache,entry,&input,1,1,NULL,NULL,NULL,NULL,error,sizeof(error));CHECK(result);xlsSemanticCheckDestroy(result);
    size_t retained=xlsEntryCacheBytes(cache);const char* newRoot="import helper\nfunc main(...) as int\n return helper.answer()+12345\nend\n";
    CHECK(writeText(entry,newRoot));cancel_data cancel={0,0};
    result=xlsSemanticCheckCreateInContext(context,entry,&input,1,1,stopAt,&cancel,error,sizeof(error));CHECK(result && cancel.calls>=6);xlsSemanticCheckDestroy(result);
    unsigned checkpoints=cancel.calls;
    for(unsigned i=1;i<=checkpoints;++i) {
        CHECK(writeText(entry,newRoot));cancel=(cancel_data){0,i};
        CHECK(!xlsSemanticCheckCreateCachedInContext(context,cache,entry,&input,1,1,NULL,NULL,stopAt,&cancel,error,sizeof(error)) && cancel.calls==i && xlsEntryCacheBytes(cache)==retained);
        CHECK(writeText(entry,root));result=xlsSemanticCheckCreateCachedInContext(context,cache,entry,&input,1,1,NULL,NULL,NULL,NULL,error,sizeof(error));
        CHECK(checkParity(result,context,entry,&input,1) && xlsSemanticCheckEntryWork(result).reused==1);xlsSemanticCheckDestroy(result);
    }
    result=xlsCheckSessionCheckInContext(session,changed,&document,1,NULL,1,NULL,NULL,error,sizeof(error));CHECK(result && xlsSemanticCheckInfo(result).accepted);
    xlsCheckSessionDestroy(session);xlsContextRelease(context);xlsContextRelease(changed);xlsEntryCacheDestroy(cache);xlsTextRelease(text);
    CHECK(xlsSemanticCheckValidate(result) && xlsSemanticCheckTypedIndex(result) && xlsSemanticCheckEntryWork(result).reused==1);
    xlsSemanticCheckDestroy(result);xlsSemanticCheckDestroy(initial);
    xrtFree(directory);xrtFree(entry);xrtFree(helper);xrtFree(package);xrtFree(manifest);xrtFree(api);return 1;
}
static int modulePrimitive(void)
{
    char* entry=xrtPathAbs("module-entry.xl");char* path=xrtPathAbs("module-ordinary.xl");CHECK(entry && path);
    const char* body="export func value() as int\n return 7\nend\n";
    size_t operations=0,charged=0;
    for(size_t failure=0;failure<=operations;++failure) {
        calls=0;failAt=failure;xls_entry_cache* cache=privateCreate(1024*1024);xls_entry_cache_scope scope;privateBegin(&scope,cache,NULL,entry);
        XL6_ParseResult result,fresh;xl6ParseResultInit(&result);xl6ParseResultInit(&fresh);
        CHECK(privateParse(body,path,&result,NULL) && xl6ParseSource(body,path,&fresh) && equal(result.root,fresh.root));
        privateEnd(&scope,1);if(!failure) {operations=calls;charged=privateModuleBytes(cache);CHECK(operations>=10 && charged>strlen(body) && scope.moduleWork.parsed==1 && !scope.moduleWork.declined);}
        xl6ParseResultFree(&result);xl6ParseResultFree(&fresh);privateDestroy(cache);CHECK(!live);
    }
    failAt=0;
    for(unsigned less=0;less<2;++less) {
        xls_entry_cache* cache=privateCreate(charged-less);CHECK(cache);xls_entry_cache_scope scope;privateBegin(&scope,cache,NULL,entry);XL6_ParseResult result;xl6ParseResultInit(&result);
        CHECK(privateParse(body,path,&result,NULL));privateEnd(&scope,1);
        CHECK(scope.moduleWork.parsed==1 && scope.moduleWork.declined==less && privateModuleBytes(cache)==(less ? 0 : charged));
        xl6ParseResultFree(&result);privateDestroy(cache);CHECK(!live);
    }
    xls_entry_cache* cache=privateCreate(8u*1024u*1024u);CHECK(cache);int okay=1;
    for(unsigned i=0;i<512 && okay;++i) {
        char name[512];snprintf(name,sizeof(name),"%s-dependency-%u.xl",entry,i);xls_entry_cache_scope scope;privateBegin(&scope,cache,NULL,entry);XL6_ParseResult result;xl6ParseResultInit(&result);
        okay=privateParse(body,name,&result,NULL);privateEnd(&scope,1);okay=okay && scope.moduleWork.parsed==1 && xrtMapCount(&cache->modules)==(i<512 ? i+1 : 512) && privateBytes(cache)+privateModuleBytes(cache)<=cache->limit;xl6ParseResultFree(&result);
    }
    CHECK(okay && xrtMapCount(&cache->modules)==512);
    char newest[512],evicted[512];snprintf(newest,sizeof(newest),"%s-dependency-0.xl",entry);snprintf(evicted,sizeof(evicted),"%s-dependency-1.xl",entry);
    xls_entry_cache_scope scope;privateBegin(&scope,cache,NULL,entry);XL6_ParseResult result;xl6ParseResultInit(&result);CHECK(privateParse(body,newest,&result,NULL));privateEnd(&scope,1);
    CHECK(scope.moduleWork.reused==1 && !scope.moduleWork.parsed);xl6ParseResultFree(&result);
    char added[512];snprintf(added,sizeof(added),"%s-dependency-512.xl",entry);
    privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);CHECK(privateParse(body,added,&result,NULL));privateEnd(&scope,1);
    CHECK(scope.moduleWork.parsed==1 && xrtMapCount(&cache->modules)==512);xl6ParseResultFree(&result);
    privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);CHECK(privateParse(body,evicted,&result,NULL));privateEnd(&scope,1);
    CHECK(scope.moduleWork.parsed==1 && !scope.moduleWork.reused && xrtMapCount(&cache->modules)==512);xl6ParseResultFree(&result);
    /* A cancelled new module and changed existing module are never installed. */
    size_t before=privateModuleBytes(cache);privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);CHECK(privateParse("func changed() as int\n return 9\nend\n",evicted,&result,NULL));privateEnd(&scope,0);xl6ParseResultFree(&result);
    CHECK(privateModuleBytes(cache)==before && xrtMapCount(&cache->modules)==512);
    privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);CHECK(privateParse(body,evicted,&result,NULL));privateEnd(&scope,1);CHECK(scope.moduleWork.reused==1);xl6ParseResultFree(&result);privateDestroy(cache);CHECK(!live);
    /* Entry plus modules share the same budget, and a new larger entry evicts
       modules only on commit. Pending abort preserves both committed stores. */
    cache=privateCreate(charged*3);CHECK(cache);
    for(unsigned i=0;i<8;++i) {
        char name[512];snprintf(name,sizeof(name),"%s-budget-%u.xl",entry,i);privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);CHECK(privateParse(body,name,&result,NULL));privateEnd(&scope,1);xl6ParseResultFree(&result);
        CHECK(privateBytes(cache)+privateModuleBytes(cache)<=cache->limit && xrtMapCount(&cache->modules)<8);
    }
    privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);CHECK(privateParse(body,entry,&result,NULL));privateEnd(&scope,1);xl6ParseResultFree(&result);
    CHECK(privateBytes(cache)>0 && privateBytes(cache)+privateModuleBytes(cache)<=cache->limit);
    /* Multiple captures in one scope obey the pending generation budget. */
    size_t retained=privateModuleBytes(cache);privateBegin(&scope,cache,NULL,entry);int all=1;
    for(unsigned i=0;i<20;++i) {
        char name[512];snprintf(name,sizeof(name),"%s-pending-%u.xl",entry,i);xl6ParseResultInit(&result);
        all=all && privateParse(body,name,&result,NULL) && scope.pendingModuleBytes+privateBytes(cache)<=cache->limit;xl6ParseResultFree(&result);
    }
    privateEnd(&scope,0);CHECK(all && privateModuleBytes(cache)==retained && scope.moduleWork.declined>0);
    /* A hit-copy failure must use the shared parser, retaining valid output. */
    calls=0;failAt=2;privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);
    char hit[512];snprintf(hit,sizeof(hit),"%s-budget-7.xl",entry);CHECK(privateParse(body,hit,&result,NULL));privateEnd(&scope,1);
    CHECK(scope.moduleWork.parsed==1 && scope.moduleWork.declined==1 && !scope.moduleWork.reused);xl6ParseResultFree(&result);failAt=0;
    privateDestroy(cache);CHECK(!live);xrtFree(entry);xrtFree(path);return 1;
}
static int moduleIncremental(void)
{
    char *entry=xrtPathAbs("incremental-entry.xl"),*path=xrtPathAbs("incremental-module.xl");CHECK(entry && path);
    const char* format="\xef\xbb\xbf#doc\r\ndesc:\r\n 注释😀\r\n#end\r\nfunc first() as int\r\n return 1\r\nend\r\nexport func changed() as int\r\n // 变化😀 %u\r\n return %u\r\nend\r\nclass Box\r\n string 名称\r\nend\r\nfunc last() as int\r\n return 3\r\nend\r\n";
    char original[1024];snprintf(original,sizeof(original),format,0u,0u);
    xls_entry_cache* cache=privateCreate(1024*1024);CHECK(cache);xls_entry_cache_scope scope;XL6_ParseResult result,fresh,retained;
    xl6ParseResultInit(&retained);privateBegin(&scope,cache,NULL,entry);CHECK(privateParse(original,path,&retained,NULL));privateEnd(&scope,1);
    CHECK(scope.moduleSyntaxWork.full==1 && !scope.moduleSyntaxWork.fragments && !scope.moduleSyntaxWork.reused);
    xls_entry_cache_data** data=xrtMapGet(&cache->modules,key(path));CHECK(data && (*data)->syntax && !(*data)->root);
    size_t bytes=0;CHECK(xlsSyntaxCacheBytes((*data)->syntax,SIZE_MAX,&bytes) && bytes>strlen(original));
    size_t exact=bytes;CHECK(xlsSyntaxCacheBytes((*data)->syntax,exact,&bytes) && bytes==exact);
    CHECK(!xlsSyntaxCacheBytes((*data)->syntax,exact-1,&bytes) && !bytes);
    xls_text* raw=xlsTextCreate(original,strlen(original),1024*1024,NULL);CHECK(raw && xlsTextStorageBytes(raw)>strlen(original));xlsTextRelease(raw);
    for(unsigned i=1;i<=20;++i) {
        char body[1024];snprintf(body,sizeof(body),format,i,i);xl6ParseResultInit(&result);xl6ParseResultInit(&fresh);
        privateBegin(&scope,cache,NULL,entry);CHECK(privateParse(body,path,&result,NULL));privateEnd(&scope,1);
        CHECK(scope.moduleWork.parsed==1 && !scope.moduleSyntaxWork.full && scope.moduleSyntaxWork.fragments==1 && scope.moduleSyntaxWork.reused==3 && scope.moduleSyntaxWork.bytes<strlen(body));
        CHECK(xl6ParseSource(body,path,&fresh) && equal(result.root,fresh.root));xl6ParseResultFree(&result);xl6ParseResultFree(&fresh);
        CHECK(privateBytes(cache)+privateModuleBytes(cache)<=cache->limit);
    }
    xl6ParseResultInit(&fresh);CHECK(xl6ParseSource(original,path,&fresh) && equal(retained.root,fresh.root));xl6ParseResultFree(&fresh);xl6ParseResultFree(&retained);
    char previous[1024],next[1024];snprintf(previous,sizeof(previous),format,20u,20u);snprintf(next,sizeof(next),format,21u,21u);
    size_t saved=privateModuleBytes(cache);privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);CHECK(privateParse(next,path,&result,NULL));privateEnd(&scope,0);xl6ParseResultFree(&result);
    CHECK(scope.moduleSyntaxWork.fragments==1 && scope.moduleSyntaxWork.reused==3 && privateModuleBytes(cache)==saved);
    privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);CHECK(privateParse(previous,path,&result,NULL));privateEnd(&scope,1);CHECK(scope.moduleWork.reused==1 && !scope.moduleWork.parsed);xl6ParseResultFree(&result);
    const char* broken="func broken(\n";privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);xl6ParseResultInit(&fresh);
    CHECK(!privateParse(broken,path,&result,NULL) && !xl6ParseSource(broken,path,&fresh));privateEnd(&scope,1);
    CHECK(result.diags.count==fresh.diags.count && result.diags.count && privateModuleBytes(cache)==saved);
    for(size_t i=0;i<result.diags.count;++i) CHECK(result.diags.items[i].line==fresh.diags.items[i].line && result.diags.items[i].column==fresh.diags.items[i].column && same(result.diags.items[i].message,fresh.diags.items[i].message));
    xl6ParseResultFree(&result);xl6ParseResultFree(&fresh);privateDestroy(cache);CHECK(!live);
    size_t operations=0;
    for(size_t failure=0;failure<=operations;++failure) {
        calls=failAt=0;cache=privateCreate(1024*1024);CHECK(cache);privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);CHECK(privateParse(original,path,&result,NULL));privateEnd(&scope,1);xl6ParseResultFree(&result);
        calls=0;failAt=failure;privateBegin(&scope,cache,NULL,entry);xl6ParseResultInit(&result);xl6ParseResultInit(&fresh);
        CHECK(privateParse(next,path,&result,NULL) && xl6ParseSource(next,path,&fresh) && equal(result.root,fresh.root));privateEnd(&scope,1);
        if(!failure) {operations=calls;CHECK(operations>=10 && scope.moduleSyntaxWork.fragments==1 && scope.moduleSyntaxWork.reused==3);}
        CHECK(privateBytes(cache)+privateModuleBytes(cache)<=cache->limit);xl6ParseResultFree(&result);xl6ParseResultFree(&fresh);privateDestroy(cache);CHECK(!live);
    }
    failAt=0;printf("Module function-update optional-operation seams=%zu,all failed/recovered\n",operations);xrtFree(entry);xrtFree(path);return 1;
}
static int moduleCheckIncremental(void)
{
    char relative[160],error[256];snprintf(relative,sizeof(relative),"build/module-function-cache-%llu",(unsigned long long)xrtClock());
    char *directory=xrtPathAbs(relative),*entry=xrtPathJoin(directory,"main.xl"),*helper=xrtPathJoin(directory,"helper.xl");
    CHECK(directory && entry && helper && xrtDirCreateAll(directory) && writeText(entry,"import helper\nfunc main(...) as int\n return helper.answer()\nend\n"));
    xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=directory,.entry=entry,.directory=directory,.configuration="debug",.analysisProfile="fixture-linked-native",.targetKind="exe"};
    xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(context && session);
    xls_semantic_check* initial=NULL;
    for(unsigned i=0;i<12;++i) {
        xstrbuf body={0};CHECK(xrtStrBufAppendFormat(&body,"// 模块😀\r\n"));
        for(unsigned j=0;j<20;++j) CHECK(xrtStrBufAppendFormat(&body,"func helper_%u() as int\r\n return %u\r\nend\r\n",j,j));
        CHECK(xrtStrBufAppendFormat(&body,"export func answer() as int\r\n return %u\r\nend\r\n",i));
        xls_text* text=xlsTextCreate(body.Data,body.Size,1024*1024,NULL);CHECK(text);
        xls_analysis_document document={.key="module",.path=helper,.text=text,.identity=1,.revision=i+1};xls_semantic_input input={helper,body.Data,body.Size};
        xls_semantic_check* result=xlsCheckSessionCheckInContext(session,context,&document,1,NULL,1,NULL,NULL,error,sizeof(error));
        CHECK(checkParity(result,context,entry,&input,1) && xlsSemanticCheckInfo(result).accepted);
        xls_parse_work work=xlsSemanticCheckModuleParseWork(result);
        CHECK(i ? !work.full && work.fragments==1 && work.reused==20 && work.bytes<body.Size : work.full==1 && !work.fragments && !work.reused);
        if(!i) initial=xlsSemanticCheckRetain(result);
        xlsSemanticCheckDestroy(result);xlsTextRelease(text);xrtStrBufFree(&body);
    }
    xlsCheckSessionDestroy(session);CHECK(initial && xlsSemanticCheckValidate(initial) && xlsSemanticCheckTypedIndex(initial));xlsSemanticCheckDestroy(initial);xlsContextRelease(context);
    xrtFree(directory);xrtFree(entry);xrtFree(helper);return 1;
}
static int cursorScope(void)
{
    char *entry=xrtPathAbs("scope-cursor-entry.xl"),*path=xrtPathAbs("scope-cursor-module.xl");CHECK(entry && path);
    const char* source="export func value() as int\n int member = 7\n return mem\nend\n";
    size_t at=(size_t)(strstr(source,"return mem")-source)+strlen("return mem");xls_text* text=xlsTextCreate(source,strlen(source),1024*1024,NULL);CHECK(text);
    XL6_CompletionQuery projection={.path=path,.offset=at};xls_parse_work work={0};xls_syntax* syntax=xlsSyntaxBuildCursor(path,text,NULL,&projection,&work);CHECK(syntax);xl6CompletionFree(&projection);
    for(unsigned mode=0;mode<3;++mode) {
        calls=failAt=0;xls_entry_cache* cache=privateCreate(1024*1024);CHECK(cache);calls=0;failAt=mode==1 ? 2 : 0;
        xls_entry_cache_scope scope;privateBegin(&scope,cache,NULL,entry);scope.cursorSyntax=syntax;
        XL6_ParseResult result;xl6ParseResultInit(&result);XL6_CompletionQuery query={.path=path,.offset=at};char changed[256];snprintf(changed,sizeof(changed),"%s",source);if(mode==2)strstr(changed,"= 7")[2]='8';
        int okay=privateParse(changed,path,&result,&query);CHECK(mode ? !okay && scope.cursorFailed && !scope.cursorApplied && !result.root && !query.prefix : okay && scope.cursorApplied && !scope.cursorFailed && result.root && query.prefix);
        privateEnd(&scope,0);xl6ParseResultFree(&result);xl6CompletionFree(&query);privateDestroy(cache);CHECK(!live);
    }
    failAt=0;xlsSyntaxRelease(syntax);xlsTextRelease(text);xrtFree(entry);xrtFree(path);return 1;
}
int main(void) {if(!primitive() || !integrated() || !checkIntegrated() || !modulePrimitive() || !moduleIncremental() || !moduleCheckIncremental() || !cursorScope()) return 1;printf("analysis_entry_cache_test passed: %u checks; pre-semantic and actual module-function AST/fresh semantic/index/IR parity,bounded chunk generations,fault/nested/cancel ownership and old retained results\n",checks);return 0;}
