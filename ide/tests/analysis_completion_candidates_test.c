#include "analysis/semantic.h"
#include "analysis/frontend_sem.h"
#include "analysis/frontend_types.h"
#include "src/type/xl6_type_model.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static xatomic32 checks;
#define CHECK(value) do {xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED);if(!(value)){fprintf(stderr,"analysis_completion_candidates_test:%d: %s\n",__LINE__,#value);return 0;}}while(0)
static int sameText(const char* a,const char* b) {return a&&b ? !strcmp(a,b) : a==b;}
static int noAliasScope(void)
{
    xls_type_alias_work before=xlsFrontendTypeAliasWork();
    XL6_AstNode *root=xl6AstNew(XL6_AST_MODULE,NULL),*type=xl6AstNewText(XL6_AST_TYPE,"unscoped",8,NULL);
    CHECK(root&&type);char* text=xl6TypeToCanonicalStringLimited(root,type,8192,128,65536);CHECK(text&&!strcmp(text,"unscoped"));
    CHECK(xlsFrontendTypeAliasWork().queries==before.queries);xrtFree(text);xl6AstFree(type);xl6AstFree(root);return 1;
}
static int corpus(const char* text,const char* marker,int indexed,int available,size_t count)
{
    char* path=xrtPathAbs("candidate-入口.xl");const char* cursor=strstr(text,marker);CHECK(path&&cursor);
    xls_semantic_input input={path,text,strlen(text)};
    xls_semantic_completion* results[4]={0};xls_sem_overload_work work[4]={{0}};xls_type_alias_work aliases[4]={{0}};
    const size_t budgets[]={8u*1024u*1024u,0,1,8192};
    for(size_t k=0;k<4;++k) {
        xls_sem_candidate_scope scope; xlsFrontendSemCandidatesBegin(&scope,budgets[k]);
        xlsFrontendTypeAliasReset();
        results[k]=xlsSemanticCompletionCreate(path,path,(size_t)(cursor-text)+strlen(marker),&input,1,NULL,NULL,NULL,0);
        xlsFrontendSemCandidatesEnd(&scope,&work[k]);aliases[k]=xlsFrontendTypeAliasWork();CHECK(results[k]&&noAliasScope());
    }
    xls_completion_info expected=xlsSemanticCompletionInfo(results[1]);char* reference=xlsSemanticCompletionJson(results[1],NULL);CHECK(reference);
    if(available>=0) CHECK(expected.available==available);
    if(count!=(size_t)-1) CHECK(expected.count==count);
    for(size_t k=0;k<4;++k) {
        xls_completion_info info=xlsSemanticCompletionInfo(results[k]);
        CHECK(info.available==expected.available&&info.offset==expected.offset&&info.start==expected.start&&info.end==expected.end&&info.count==expected.count);
        CHECK(sameText(info.path,expected.path)&&sameText(info.prefix,expected.prefix)&&sameText(info.error,expected.error));
        char* json=xlsSemanticCompletionJson(results[k],NULL);CHECK(json&&!strcmp(json,reference));xlsSemanticStringFree(json);
        CHECK(work[k].bytes<=budgets[k]&&work[k].pairVisits<=work[1].pairVisits);
        CHECK(aliases[k].peakBytes<=budgets[k]);
        for(size_t i=0;i<info.count;++i) {
            xls_completion_item a,b;CHECK(xlsSemanticCompletionItem(results[k],i,&a)&&xlsSemanticCompletionItem(results[1],i,&b));
            CHECK(sameText(a.label,b.label)&&sameText(a.detail,b.detail)&&sameText(a.kind,b.kind)&&sameText(a.selectionPath,b.selectionPath));
            CHECK(a.symbol==b.symbol&&a.selectionStart==b.selectionStart&&a.selectionEnd==b.selectionEnd&&a.hasSelection==b.hasSelection);
        }
    }
    CHECK(!work[1].indexes&&!work[2].indexes&&work[1].pairVisits==work[2].pairVisits);
    CHECK(!aliases[1].indexes&&!aliases[1].indexedQueries);
    if(indexed==3) CHECK(!expected.available&&expected.error&&strstr(expected.error,"bounded semantic completion type")&&aliases[0].queries);
    if(aliases[0].queries) {
        CHECK(aliases[0].indexes==1&&aliases[0].indexedQueries==aliases[0].queries&&!aliases[0].linearQueries);
        CHECK(aliases[1].linearQueries==aliases[1].queries&&aliases[1].queries==aliases[0].queries);
        if(aliases[0].peakBytes>8192) CHECK(!aliases[3].indexes&&aliases[3].linearQueries==aliases[3].queries);
        if(aliases[0].aliases) CHECK(!aliases[2].indexes&&aliases[2].linearQueries==aliases[2].queries);
    }
    if(indexed>0) {
        CHECK(work[0].indexes==2&&!work[3].indexes&&work[1].pairVisits>work[0].pairVisits*10);
        if(indexed==1) printf("completion/conflict candidates indexed=%llu reference=%llu, indexes=%llu, largest completed data charge=%llu bytes\n",
            (unsigned long long)work[0].pairVisits,(unsigned long long)work[1].pairVisits,(unsigned long long)work[0].indexes,(unsigned long long)work[0].bytes);
    }
    if(indexed<0) CHECK(!work[0].indexes);
    for(size_t k=0;k<4;++k) xlsSemanticCompletionDestroy(results[k]);
    xlsSemanticStringFree(reference);xrtFree(path);return 1;
}
static int padded(const char* declarations,const char* body,const char* marker,int indexed,int available,size_t count)
{
    xstrbuf source;xrtStrBufInit(&source);int okay=1;
    for(unsigned i=0;okay&&i<256;++i)okay=xrtStrBufAppendFormat(&source,"func helper_%u(int value) as int\n return value + 1\nend\n",i);
    okay=okay&&xrtStrBufAppend(&source,xrtStrView(declarations))&&xrtStrBufAppend(&source,xrtStrView(body));
    CHECK(okay&&corpus(source.Data,marker,indexed,available,count));xrtStrBufFree(&source);return 1;
}
typedef struct reader {int passed;} reader;
static int32 parallel(void* user)
{
    reader* r=user;r->passed=padded("typedef Count = int\nfunc 求值(Count value) as int\n return value\nend\nfunc 求值(string value) as int\n return 0\nend\n",
        "func main(...) as int\n return 求\nend\n","return 求",2,1,2);return 0;
}
static int run(void)
{
    const char* main="func main(...) as int\n int result = 7\n return res\nend\n";
    CHECK(corpus(main,"return res",0,1,1));CHECK(padded("",main,"return res",1,1,1));
    CHECK(padded("","func main(...) as int\n return helper_\nend\n","return helper_",2,1,256));
    CHECK(padded("func choose(int value) as int\n return value\nend\nfunc choose(string value) as int\n return 0\nend\n", "func main(...) as int\n return cho\nend\n","return cho",2,1,2));
    CHECK(padded("","func main(...) as int\n int helper_17 = 7\n return helper_17\nend\n","return helper_17",2,1,11));
    CHECK(padded("typedef Count = int\nfunc chosen(Count value) as Count\n return value\nend\n", "func main(...) as int\n return chos\nend\n","return chos",2,1,1));
    CHECK(padded("typedef 元素 = int\ntypedef Alias = 元素\nclass Box<T>\n T value\nend\nfunc chosen(Alias value) as Alias\n return value\nend\n", "func main(...) as int\n return chos\nend\n","return chos",2,1,1));
    xstrbuf manyAliases;xrtStrBufInit(&manyAliases);
    for(unsigned i=0;i<1025;++i) CHECK(xrtStrBufAppendFormat(&manyAliases,"typedef Alias_%u = int\n",i));
    CHECK(padded(manyAliases.Data,main,"return res",2,1,1));xrtStrBufFree(&manyAliases);
    xstrbuf hugeType;xrtStrBufInit(&hugeType);char* longType=xrtMalloc(8201);CHECK(longType);memset(longType,'Z',8200);longType[8200]=0;
    CHECK(xrtStrBufAppendFormat(&hugeType,"class %s\nend\n",longType)&&padded(hugeType.Data,main,"return res",3,0,0));xrtFree(longType);xrtStrBufFree(&hugeType);
    CHECK(padded("func chosen<T>(T value) as T\n return value\nend\n", "func main(...) as int\n return chos\nend\n","return chos",2,1,1));
    CHECK(padded("extern \"C\" func chosen(c_int value) as c_int\n", "func main(...) as int\n return chos\nend\n","return chos",2,1,1));
    CHECK(padded("export func chosen(int value) as int\n return value\nend\n", "func main(...) as int\n return chos\nend\n","return chos",2,1,1));
    CHECK(padded("async func chosen(int value) as int\n return value\nend\n", "func main(...) as int\n return chos\nend\n","return chos",2,1,1));
    CHECK(padded("func broken() as int\n return missingName\nend\n",main,"return res",0,0,0));
    CHECK(padded("","func main(...) as int\n return nonexistentPrefix\nend\n","return nonexistentPrefix",2,1,0));
    char longName[301];memset(longName,'z',300);longName[300]=0;xstrbuf extra;xrtStrBufInit(&extra);
    CHECK(xrtStrBufAppendFormat(&extra,"func %s() as int\n return 0\nend\n",longName)&&padded(extra.Data,main,"return res",-1,1,1));xrtStrBufFree(&extra);
    /* End the inner index-enabled query, then run a real unwrapped query
     * under the outer disabled setting. Both produce the same owned report. */
    xstrbuf nested;xrtStrBufInit(&nested);
    for(unsigned i=0;i<96;++i)CHECK(xrtStrBufAppendFormat(&nested,"func nested_%u() as int\n return 0\nend\n",i));
    CHECK(xrtStrBufAppend(&nested,xrtStrView(main)));char* path=xrtPathAbs("candidate-nested.xl");CHECK(path);
    const char* cursor=strstr(nested.Data,"return res");CHECK(cursor);xls_semantic_input input={path,nested.Data,nested.Size};
    xls_sem_candidate_scope outer,inner;xls_sem_overload_work work;
    xlsFrontendSemCandidatesBegin(&outer,0);xlsFrontendSemCandidatesBegin(&inner,8u*1024u*1024u);
    xls_semantic_completion* first=xlsSemanticCompletionCreate(path,path,(size_t)(cursor-nested.Data)+10,&input,1,NULL,NULL,NULL,0);
    xlsFrontendSemCandidatesEnd(&inner,&work);CHECK(first&&work.indexes==2);
    xls_semantic_completion* second=xlsSemanticCompletionCreate(path,path,(size_t)(cursor-nested.Data)+10,&input,1,NULL,NULL,NULL,0);
    xlsFrontendSemCandidatesEnd(&outer,&work);CHECK(second&&!work.indexes);
    char *one=xlsSemanticCompletionJson(first,NULL),*two=xlsSemanticCompletionJson(second,NULL);CHECK(one&&two&&!strcmp(one,two));
    xlsSemanticStringFree(one);xlsSemanticStringFree(two);xlsSemanticCompletionDestroy(first);xlsSemanticCompletionDestroy(second);xrtFree(path);xrtStrBufFree(&nested);
    reader values[4]={{0}};xthread* threads[4];
    for(size_t i=0;i<4;++i){threads[i]=xrtThreadCreate(parallel,&values[i],0);CHECK(threads[i]);}
    for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&values[i].passed);xrtThreadDestroy(threads[i]);}
    return 1;
}
int main(void)
{
    xrtAtomic32Init(&checks,0);if(!run())return 1;
    printf("analysis_completion_candidates_test passed: %u checks\n",(unsigned)xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
